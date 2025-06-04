#include <iostream>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cstring>
#include <vector>
#include <ctime>
#include <map>
#include <pthread.h>
#include <sstream>

using namespace std;

enum FlightType { COMMERCIAL, CARGO, EMERGENCY };           //ENUMS FLightType , Same as what is in ATC file

struct AVN        //it is also same as the AVN in ATC file, kyunkay same data pipe say lakr aaaana hoata   (same as avn.cpp)
    {
        char avnID[32];
        char airlineName[32];
        char flightNumber[16];
        FlightType type;
        float speedRecorded;
        float permissibleSpeed;
        time_t issuanceTime;
        double fineAmount;
        char paymentStatus[16];
        time_t dueDate;

        void serialize(char* buffer, int& offset) const      //this function is used to convert the AVN object data into stream of bytes
            {                                                   //-hence necessary to transfer data through pipe or fifio
                memcpy(buffer + offset, avnID, sizeof(avnID));
                offset += sizeof(avnID);
                memcpy(buffer + offset, airlineName, sizeof(airlineName));
                offset += sizeof(airlineName);
                memcpy(buffer + offset, flightNumber, sizeof(flightNumber));
                offset += sizeof(flightNumber);
                memcpy(buffer + offset, &type, sizeof(type));
                offset += sizeof(type);
                memcpy(buffer + offset, &speedRecorded, sizeof(speedRecorded));
                offset += sizeof(speedRecorded);
                memcpy(buffer + offset, &permissibleSpeed, sizeof(permissibleSpeed));
                offset += sizeof(permissibleSpeed);
                memcpy(buffer + offset, &issuanceTime, sizeof(issuanceTime));
                offset += sizeof(issuanceTime);
                memcpy(buffer + offset, &fineAmount, sizeof(fineAmount));
                offset += sizeof(fineAmount);
                memcpy(buffer + offset, paymentStatus, sizeof(paymentStatus));
                offset += sizeof(paymentStatus);
                memcpy(buffer + offset, &dueDate, sizeof(dueDate));
                offset += sizeof(dueDate);
            }

        void deserialize(const char* buffer, int& offset)            //to convert back to it's original object form
            {
                memcpy(avnID, buffer + offset, sizeof(avnID));
                offset += sizeof(avnID);
                memcpy(airlineName, buffer + offset, sizeof(airlineName));
                offset += sizeof(airlineName);
                memcpy(flightNumber, buffer + offset, sizeof(flightNumber));
                offset += sizeof(flightNumber);
                memcpy(&type, buffer + offset, sizeof(type));
                offset += sizeof(type);
                memcpy(&speedRecorded, buffer + offset, sizeof(speedRecorded));
                offset += sizeof(speedRecorded);
                memcpy(&permissibleSpeed, buffer + offset, sizeof(permissibleSpeed));
                offset += sizeof(permissibleSpeed);
                memcpy(&issuanceTime, buffer + offset, sizeof(issuanceTime));
                offset += sizeof(issuanceTime);
                memcpy(&fineAmount, buffer + offset, sizeof(fineAmount));
                offset += sizeof(fineAmount);
                memcpy(paymentStatus, buffer + offset, sizeof(paymentStatus));
                offset += sizeof(paymentStatus);
                memcpy(&dueDate, buffer + offset, sizeof(dueDate));
                offset += sizeof(dueDate);
            }

        static int serializedSize()          //funciton to calculate and return size of stream produced by serialization funciton
            {
                return sizeof(avnID) + sizeof(airlineName) + sizeof(flightNumber) +
                    sizeof(type) + sizeof(speedRecorded) + sizeof(permissibleSpeed) +
                    sizeof(issuanceTime) + sizeof(fineAmount) + sizeof(paymentStatus) +
                    sizeof(dueDate);
            }
    };

struct PaymentConfirmation          //struct for payment details such as avnID, flightNum and ispaid (same as avn.cpp)
    {
        char avnID[32];
        char flightNumber[16];
        bool paymentSuccessful;

        void serialize(char* buffer, int& offset) const         //converting 
            {
                memcpy(buffer + offset, avnID, sizeof(avnID));
                offset += sizeof(avnID);
                memcpy(buffer + offset, flightNumber, sizeof(flightNumber));
                offset += sizeof(flightNumber);
                memcpy(buffer + offset, &paymentSuccessful, sizeof(paymentSuccessful));
                offset += sizeof(paymentSuccessful);
            }

        void deserialize(const char* buffer, int& offset)   //convertingf back to object
            {
                memcpy(avnID, buffer + offset, sizeof(avnID));
                offset += sizeof(avnID);
                memcpy(flightNumber, buffer + offset, sizeof(flightNumber));
                offset += sizeof(flightNumber);
                memcpy(&paymentSuccessful, buffer + offset, sizeof(paymentSuccessful));
                offset += sizeof(paymentSuccessful);
            }

        static int serializedSize() 
            {
                return sizeof(avnID) + sizeof(flightNumber) + sizeof(paymentSuccessful);
            }
    };

class AirlinePortal 
    {
        private:
            map<string, AVN> avnRecords; //storing AVNs by avnID in a hashmap
            pthread_mutex_t avnMutex;
            int fdAvnPipe = -1;        //for reading from avn_to_airline.fifo
            int fdStripePipe = -1;     //for reading from stripe_to_airline.fifo
            map<string, string> airlineCredentials; //to store airline credentials
            string loggedInAirline; //to track the currently loggedin airline

            string flightTypeToStr(FlightType f) 
                {
                    switch(f)       //converting enum flight type to string
                        {
                            case COMMERCIAL: 
                                return "Commercial";
                            case CARGO: 
                                return "Cargo";
                            case EMERGENCY: 
                                return "Emergency";
                            default: 
                                return "Unknown";
                        }
                }

            void displayAVNs(bool filterByAirline = false, const string& flightNumber = "", time_t searchTime = 0)      //function to display all avns of an airline
                {
                    pthread_mutex_lock(&avnMutex);
                    cout << "\n===== Airline Portal: Active and Historical AVNs for " << loggedInAirline << " =====\n";
                    bool found = false;
                    for(const auto& entry : avnRecords) 
                        {
                            const AVN& avn = entry.second;
                            //filter by airline name
                            if(filterByAirline && strcmp(avn.airlineName, loggedInAirline.c_str()) != 0) 
                                {
                                    continue;
                                }
                            //filter by flight number and issuance time if provided
                            if(!flightNumber.empty() && strcmp(avn.flightNumber, flightNumber.c_str()) != 0) 
                                {
                                    continue;
                                }
                            if(searchTime != 0) 
                                {
                                    //allow a small time window (e.g., 60 seconds) for matching issuance time
                                    if(abs(difftime(searchTime, avn.issuanceTime)) > 60) 
                                        {
                                            continue;
                                        }
                                }
                            found = true;
                            time_t now = time(nullptr);
                            bool isOverdue = now > avn.dueDate && strcmp(avn.paymentStatus, "paid") != 0;
                            cout << "AVN ID: " << avn.avnID << "\n"
                                << "Aircraft ID (Flight Number): " << avn.flightNumber << "\n"
                                << "Aircraft Type: " << flightTypeToStr(avn.type) << "\n"
                                << "Payment Status: " << avn.paymentStatus << (isOverdue ? " (Overdue)" : "") << "\n"
                                << "Total Fine Amount: PKR " << avn.fineAmount << "\n"
                                << "Issuance Date/Time: " << ctime(&avn.issuanceTime)
                                << "Due Date: " << ctime(&avn.dueDate) << "\n"
                                << "--------------------------------\n";
                        }
                    if(!found) 
                        {
                            cout << "No matching AVNs found.\n";
                        }
                    cout << "==============================================\n" << flush;
                    pthread_mutex_unlock(&avnMutex);
                }

            time_t parseDateTime(const string& dateTimeStr)     //setting date-time format
                {
                    struct tm tm = {0};
                    if(strptime(dateTimeStr.c_str(), "%Y-%m-%d %H:%M:%S", &tm) == nullptr) {
                        cout << "[ERROR] Invalid date-time format. Use YYYY-MM-DD HH:MM:SS\n" << flush;
                        return 0;
                    }
                    return mktime(&tm);
                }

            bool authenticate()         //authentication to airline portal
                {
                    while(1)
                        {
                            string username, password;
                            cout << "[Airline Portal] Enter airline name: ";
                            getline(cin, username);
                            cout << "[Airline Portal] Enter password: ";
                            getline(cin, password);

                            auto it = airlineCredentials.find(username);
                            if(it != airlineCredentials.end() && it->second == password) 
                                {
                                    loggedInAirline = username;
                                    cout << "[Airline Portal] Successfully logged in as " << username << "\n" << flush;
                                    return true;
                                } 
                            else 
                                {
                                    cout << "[Airline Portal] Invalid credentials. Try again or use 'relogin' if needed.\n" << flush;
                                }
                        }  
                }

        public:
            AirlinePortal() 
                {
                    pthread_mutex_init(&avnMutex, nullptr);
                    cout << "[Airline Portal] Starting initialization...\n" << flush;

                    //setting airline credentials (username: airline name, password)
                    airlineCredentials["PIA"] = "pia123";
                    airlineCredentials["AirBlue"] = "qatar123";
                    airlineCredentials["Pakistan Airforce"] = "paf123";
                    airlineCredentials["FedEx"] = "fedex123";
                    airlineCredentials["Blue Dart"] = "bluedart123";
                    airlineCredentials["AghaKhan Air"] = "ak123";

                    //opeing avn_to_airline.fifo for reading
                    for(int attempt = 1; attempt <= 20; attempt++) 
                        {
                            fdAvnPipe = open("avn_to_airline.fifo", O_RDONLY | O_NONBLOCK);
                            if(fdAvnPipe < 0) 
                                {
                                    cout << "[Airline Portal] Attempt " << attempt << " failed to open avn_to_airline.fifo: " << strerror(errno) << ", retrying...\n" << flush;
                                    usleep(500000);
                                    continue;
                                }
                            cout << "[Airline Portal] Successfully opened avn_to_airline.fifo\n" << flush;
                            break;
                        }
                    if(fdAvnPipe < 0) 
                        {
                            cout << "[ERROR] Failed to open avn_to_airline.fifo after retries: " << strerror(errno) << endl << flush;
                            exit(1);
                        }

                    //opening stripe_to_airline.fifo for reading
                    for(int attempt = 1; attempt <= 20; attempt++) 
                        {
                            fdStripePipe = open("stripe_to_airline.fifo", O_RDONLY | O_NONBLOCK);
                            if(fdStripePipe < 0) 
                                {
                                    cout << "[Airline Portal] Attempt " << attempt << " failed to open stripe_to_airline.fifo: " << strerror(errno) << ", retrying...\n" << flush;
                                    usleep(500000);
                                    continue;
                                }
                            cout << "[Airline Portal] Successfully opened stripe_to_airline.fifo\n" << flush;
                            break;
                        }
                    if(fdStripePipe < 0) 
                        {
                            cout << "[ERROR] Failed to open stripe_to_airline.fifo after retries: " << strerror(errno) << endl << flush;
                            close(fdAvnPipe);
                            exit(1);
                        }

                    //authenticating user before proceeding
                    if(!authenticate()) 
                        {
                            close(fdAvnPipe);
                            close(fdStripePipe);
                            pthread_mutex_destroy(&avnMutex);
                            exit(1);
                        }

                    cout << "[Airline Portal] Initialization complete.\n" << flush;
                }

            void processAVN() 
                {
                    char buffer[AVN::serializedSize()];
                    int bytesRead = read(fdAvnPipe, buffer, sizeof(buffer));

                    if(bytesRead == sizeof(buffer)) 
                        {
                            AVN avn;
                            int offset = 0;
                            avn.deserialize(buffer, offset);

                            pthread_mutex_lock(&avnMutex);
                            avnRecords[avn.avnID] = avn;
                            pthread_mutex_unlock(&avnMutex);

                            cout << "[Airline Portal] Received AVN: " << avn.avnID << ", Flight: " << avn.flightNumber << ", Amount: PKR " << avn.fineAmount << endl << flush;
                            displayAVNs(true);
                        } 
                    else if(bytesRead < 0 && errno != EAGAIN) 
                        {
                            cout << "[ERROR] Failed to read from avn_to_airline.fifo: " << strerror(errno) << endl << flush;
                        }
                }

            void confirmPayment()   
                {
                    char buffer[PaymentConfirmation::serializedSize()];
                    int bytesRead = read(fdStripePipe, buffer, sizeof(buffer));

                    if(bytesRead == sizeof(buffer)) 
                        {
                            PaymentConfirmation confirmation;
                            int offset = 0;
                            confirmation.deserialize(buffer, offset);

                            if(confirmation.paymentSuccessful) 
                                {
                                    pthread_mutex_lock(&avnMutex);
                                    auto i = avnRecords.find(confirmation.avnID);
                                    if(i != avnRecords.end()) 
                                        {
                                            AVN& avn = i->second;
                                            strncpy(avn.paymentStatus, "paid", sizeof(avn.paymentStatus) - 1);
                                            avn.paymentStatus[sizeof(avn.paymentStatus) - 1] = '\0';

                                            cout << "[Airline Portal] Updated payment status to 'paid' for AVN " << avn.avnID << ", Flight: " << avn.flightNumber << endl << flush;
                                            displayAVNs(true); //display AVNs for the logged-in airline only
                                        } 
                                    else 
                                        {
                                            cout << "[ERROR] AVN " << confirmation.avnID << " not found in records\n" << flush;
                                        }
                                    pthread_mutex_unlock(&avnMutex);
                                } 
                            else 
                                {
                                    cout << "[Airline Portal] Payment failed for AVN " << confirmation.avnID << ", Flight: " << confirmation.flightNumber << endl << flush;
                                }
                        } 
                    else if(bytesRead < 0 && errno != EAGAIN) 
                        {
                            cout << "[ERROR] Failed to read from stripe_to_airline.fifo: " << strerror(errno) << endl << flush;
                        }
                }

            void run() 
                {
                    cout << "[Airline Portal] Starting main loop...\n" << flush;
                    cout << "[Airline Portal] Commands:\n"
                        << "  view - Display all AVNs for your airline\n"
                        << "  search <flightNumber> <issuanceDateTime> - Search AVNs by flight number and issuance date/time (format: YYYY-MM-DD HH:MM:SS)\n"
                        << "  relogin - Log out and log in as a different airline\n"
                        << "  exit - Quit the portal\n"
                        << "Press Enter to continue polling.\n" << flush;

                    while(true) 
                        {
                            cout << "[Airline Portal] Enter command: ";
                            string commandLine;
                            getline(cin, commandLine);

                            istringstream iss(commandLine);
                            string command;
                            iss >> command;

                            if(command == "view") 
                                {
                                    displayAVNs(true); //displaying AVNs for the logged-in airline only
                                } 
                            else if(command == "search") 
                                {
                                    string flightNumber, dateTimeStr;
                                    iss >> flightNumber;
                                    getline(iss, dateTimeStr); //get the data of all flights
                                    dateTimeStr = dateTimeStr.substr(dateTimeStr.find_first_not_of(" ")); // Trim leading spaces

                                    time_t searchTime = 0;
                                    if(!dateTimeStr.empty()) 
                                        {
                                            searchTime = parseDateTime(dateTimeStr);
                                            if(searchTime == 0) 
                                                {
                                                    continue; // Invalid date-time format, skip processing
                                                }
                                        }
                                    displayAVNs(true, flightNumber, searchTime);
                                } 
                            else if(command == "relogin") 
                                {
                                    loggedInAirline.clear(); // Clear the current login
                                    if(!authenticate()) 
                                        {
                                            break; // Exit if authentication fails after relogin attempt
                                        }
                                } 
                            else if(command == "exit") 
                                {
                                    break;
                                }

                            // Process incoming AVNs and payment confirmations
                            processAVN();
                            confirmPayment();

                            usleep(100000); // Sleep for 100ms to prevent busy-waiting
                        }
                }

            ~AirlinePortal() 
                {
                    cout << "[Airline Portal] Cleaning up...\n" << flush;
                    if(fdAvnPipe >= 0) 
                        close(fdAvnPipe);
                    if(fdStripePipe >= 0) 
                        close(fdStripePipe);
                    pthread_mutex_destroy(&avnMutex);
                    cout << "[Airline Portal] Shutdown complete.\n" << flush;
                }
    };

int main() 
    {
        cout << "===== Airline Portal Starting =====\n" << flush;
        AirlinePortal portal;
        portal.run();
        cout << "===== Airline Portal Complete =====\n" << flush;
        return 0;
    }
