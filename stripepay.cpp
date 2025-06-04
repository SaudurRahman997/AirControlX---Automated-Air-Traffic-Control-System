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

class StripePay 
    {
        private:
            int fdAvnPipe = -1;        //for reading from avn_to_stripe.fifo
            int fdAvnConfirmPipe = -1; //for writing to stripe_to_avn.fifo
            int fdAirlineConfirmPipe = -1; //for writing to stripe_to_airline.fifo
            map<string, string> airlineCredentials; //for storing airline credentials
            string loggedInAirline; //for tracking the currently logged-in airline

            string flightTypeToStr(FlightType f) 
                {
                    switch(f) 
                        {
                            case COMMERCIAL: return "Commercial";
                            case CARGO: return "Cargo";
                            case EMERGENCY: return "Emergency";
                            default: return "Unknown";
                        }
                }

            bool authenticate()         //function to authenticate airline admins credentials
                {
                    while(1)
                        {
                            string username, password;
                            cout << "[StripePay] Enter airline name: ";
                            getline(cin, username);
                            cout << "[StripePay] Enter password: ";
                            getline(cin, password);

                            auto it = airlineCredentials.find(username);
                            if(it != airlineCredentials.end() && it->second == password) 
                                {
                                    loggedInAirline = username;
                                    cout << "[StripePay] Successfully logged in as " << username << "\n" << flush;
                                    return true;
                                } 
                            else 
                            {
                                cout << "[StripePay] Invalid credentials. Try again or use 'relogin' if needed.\n" << flush;                                
                            }
                        }
                }

        public:
            StripePay() 
                {
                    pthread_mutex_t mutex; // Dummy mutex for compilation (not used in this version)
                    pthread_mutex_init(&mutex, nullptr);
                    cout << "[StripePay] Starting initialization...\n" << flush;

                    // Initialize airline credentials (username: airline name, password)
                    airlineCredentials["PIA"] = "pia123";
                    airlineCredentials["AirBlue"] = "qatar123";
                    airlineCredentials["Pakistan Airforce"] = "paf123";
                    airlineCredentials["FedEx"] = "fedex123";
                    airlineCredentials["Blue Dart"] = "bluedart123";
                    airlineCredentials["AghaKhan Air"] = "ak123";

                    // Create new FIFO for communication with Airline Portal
                    if(mkfifo("stripe_to_airline.fifo", 0666) == -1 && errno != EEXIST) 
                        {
                            cout << "[ERROR] Failed to create stripe_to_airline.fifo: " << strerror(errno) << endl << flush;
                            exit(1);
                        }

                    for(int attempt = 1; attempt <= 20; attempt++)        //ifAVN file not opened the nit will wait and retry 20 times until writer avn.cpp opens
                        {
                            fdAvnPipe = open("avn_to_stripe.fifo", O_RDONLY | O_NONBLOCK);
                            if(fdAvnPipe < 0) 
                                {
                                    cout << "[StripePay] Attempt " << attempt << " failed to open avn_to_stripe.fifo: " << strerror(errno) << ", retrying...\n" << flush;
                                    usleep(500000);
                                    continue;
                                }
                            cout << "[StripePay] Successfully opened avn_to_stripe.fifo\n" << flush;
                            break;
                        }
                    if(fdAvnPipe < 0) 
                        {
                            cout << "[ERROR] Failed to open avn_to_stripe.fifo after retries: " << strerror(errno) << endl << flush;
                            exit(1);
                        }   

                    for(int attempt = 1; attempt <= 20; attempt++)        //ifAVN file not opened the nit will wait and retry 20 times until reader avn.cpp opens
                        {
                            fdAvnConfirmPipe = open("stripe_to_avn.fifo", O_WRONLY | O_NONBLOCK);
                            if(fdAvnConfirmPipe < 0) 
                                {
                                    cout << "[StripePay] Attempt " << attempt << " failed to open stripe_to_avn.fifo: " << strerror(errno) << ", retrying...\n" << flush;
                                    usleep(500000);
                                    continue;
                                }
                            cout << "[StripePay] Successfully opened stripe_to_avn.fifo\n" << flush;
                            break;
                        }
                    if(fdAvnConfirmPipe < 0) 
                        {
                            cout << "[ERROR] Failed to open stripe_to_avn.fifo after retries: " << strerror(errno) << endl << flush;
                            close(fdAvnPipe);
                            exit(1);
                        }

                    //opening stripe_to_airline.fifo for writing
                    for(int attempt = 1; attempt <= 10; attempt++) 
                        {
                            fdAirlineConfirmPipe = open("stripe_to_airline.fifo", O_WRONLY | O_NONBLOCK);
                            if(fdAirlineConfirmPipe < 0) 
                                {
                                    cout << "[StripePay] Attempt " << attempt << " failed to open stripe_to_airline.fifo: " << strerror(errno) << ", retrying...\n" << flush;
                                    usleep(500000);
                                    continue;
                                }
                            cout << "[StripePay] Successfully opened stripe_to_airline.fifo\n" << flush;
                            break;
                        }
                    if(fdAirlineConfirmPipe < 0) 
                        {
                            cout << "[ERROR] Failed to open stripe_to_airline.fifo after retries: " << strerror(errno) << endl << flush;
                            close(fdAvnPipe);
                            close(fdAvnConfirmPipe);
                            exit(1);
                        }

                    //authenticating user before proceeding
                    if(!authenticate()) 
                        {
                            close(fdAvnPipe);
                            close(fdAvnConfirmPipe);
                            close(fdAirlineConfirmPipe);
                            exit(1);
                        }

                    cout << "[StripePay] Initialization complete.\n" << flush;
                }

            void processPayment()       //processing the upcoming payment
                {
                    char buffer[AVN::serializedSize()];
                    int bytesRead = read(fdAvnPipe, buffer, sizeof(buffer));

                    if(bytesRead == sizeof(buffer)) 
                        {
                            AVN avn;
                            int offset = 0;
                            avn.deserialize(buffer, offset);

                            cout << "[StripePay] Received AVN: " << avn.avnID << ", Flight: " << avn.flightNumber << ", Type: " << flightTypeToStr(avn.type) << ", Amount: PKR " << avn.fineAmount << endl << flush;

                            // Check if the AVN belongs to the logged-in airline
                            if(strcmp(avn.airlineName, loggedInAirline.c_str()) != 0) 
                                {
                                    cout << "[StripePay] Unauthorized: AVN " << avn.avnID << " belongs to " << avn.airlineName << ", not " << loggedInAirline << ". Payment not allowed.\n" << flush;
                                    return;
                                }

                            //simulating airline admin payment
                            double paidAmount;
                            cout << "[StripePay] Please enter the payment amount for AVN " << avn.avnID << " (expected: PKR " << avn.fineAmount << "): ";
                            cin >> paidAmount;

                            bool paymentSuccessful = (abs(paidAmount - avn.fineAmount) < 0.01); //allow for minor floating-point differences
                            if(paymentSuccessful)   //if payment done
                                {
                                    cout << "[StripePay] Payment of PKR " << paidAmount << " accepted for AVN " << avn.avnID << endl << flush;
                                } 
                            else 
                                {
                                    cout << "[StripePay] Payment failed: Expected PKR " << avn.fineAmount << ", received PKR " << paidAmount << endl << flush;
                                }

                            //senfing payment confirmation to AVN Generator
                            PaymentConfirmation confirmation;
                            strncpy(confirmation.avnID, avn.avnID, sizeof(confirmation.avnID) - 1);
                            confirmation.avnID[sizeof(confirmation.avnID) - 1] = '\0';
                            strncpy(confirmation.flightNumber, avn.flightNumber, sizeof(confirmation.flightNumber) - 1);
                            confirmation.flightNumber[sizeof(confirmation.flightNumber) - 1] = '\0';
                            confirmation.paymentSuccessful = paymentSuccessful;

                            char confirmBuffer[PaymentConfirmation::serializedSize()];
                            offset = 0;
                            confirmation.serialize(confirmBuffer, offset);

                            int bytesWritten = write(fdAvnConfirmPipe, confirmBuffer, sizeof(confirmBuffer));
                            if(bytesWritten == sizeof(confirmBuffer)) 
                                {
                                    cout << "[StripePay] Sent payment confirmation to AVN Generator for AVN " << avn.avnID << " (status: " << (paymentSuccessful ? "success" : "failure") << ")\n" << flush;
                                } 
                            else 
                                {
                                    cout << "[ERROR] Failed to send payment confirmation to AVN Generator for AVN " << avn.avnID << ", bytes written: " << bytesWritten << ", expected: " << sizeof(confirmBuffer) << ", error: " << strerror(errno) << endl << flush;
                                }

                            //confirming payment to Airline Portal
                            bytesWritten = write(fdAirlineConfirmPipe, confirmBuffer, sizeof(confirmBuffer));
                            if(bytesWritten == sizeof(confirmBuffer)) 
                                {
                                    cout << "[StripePay] Sent payment confirmation to Airline Portal for AVN " << avn.avnID << " (status: " << (paymentSuccessful ? "success" : "failure") << ")\n" << flush;
                                } 
                            else 
                                {
                                    cout << "[ERROR] Failed to send payment confirmation to Airline Portal for AVN " << avn.avnID << ", bytes written: " << bytesWritten << ", expected: " << sizeof(confirmBuffer) << ", error: " << strerror(errno) << endl << flush;
                                }
                        } 
                    else if(bytesRead < 0 && errno != EAGAIN) 
                        {
                            cout << "[ERROR] Failed to read from avn_to_stripe.fifo: " << strerror(errno) << endl << flush;
                        }
                }

            void run() 
                {
                    cout << "[StripePay] Commands:\n"
                        << "  relogin - Log out and log in as a different airline\n"
                        << "  exit - Quit the process\n"
                        << "Press Enter to continue polling.\n" << flush;

                    while(true) 
                        {
                            cout << "[StripePay] Enter command: ";
                            string commandLine;
                            getline(cin, commandLine);

                            istringstream iss(commandLine);
                            string command;
                            iss >> command;

                            if(command == "relogin") 
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

                            processPayment();
                            usleep(100000); // Sleep for 100ms to prevent busy-waiting
                        }
                }

            ~StripePay()    //closing all pipes
                {
                    if(fdAvnPipe >= 0) 
                        close(fdAvnPipe);
                    if(fdAvnConfirmPipe >= 0) 
                        close(fdAvnConfirmPipe);
                    if(fdAirlineConfirmPipe >= 0) 
                        close(fdAirlineConfirmPipe);                   
                }
    };

int main() 
    {
        cout << "===== StripePay Starting =====\n" << flush;
        StripePay stripePay;
        stripePay.run();
        cout << "===== StripePay Complete =====\n" << flush;
        return 0;
    }
