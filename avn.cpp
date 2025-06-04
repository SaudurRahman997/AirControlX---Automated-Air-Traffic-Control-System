#include <iostream>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cstring>
#include <vector>
#include <ctime>
#include <map>
#include <pthread.h>

using namespace std;

enum FlightType { COMMERCIAL, CARGO, EMERGENCY };           //ENUMS FLightType , Same as what is in ATC file

struct AVN        //it is also same as the AVN in ATC file, kyunkay same data pipe say lakr aaaana hoata
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

struct PaymentConfirmation          //struct for payment details such as avnID, flightNum and ispaid
    {
        char avnID[32];
        char flightNumber[16];
        bool paymentSuccessful;

        void serialize(char* buffer, int& offset) const 
            {
                memcpy(buffer + offset, avnID, sizeof(avnID));
                offset += sizeof(avnID);
                memcpy(buffer + offset, flightNumber, sizeof(flightNumber));
                offset += sizeof(flightNumber);
                memcpy(buffer + offset, &paymentSuccessful, sizeof(paymentSuccessful));
                offset += sizeof(paymentSuccessful);
            }

        void deserialize(const char* buffer, int& offset) 
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

struct ViolationClearedNotification             //isVioiation cleared (this will be communicated through avnToAtc pipe)
    {
        char avnID[32];
        char flightNumber[16];

        void serialize(char* buffer, int& offset) const 
            {
                memcpy(buffer + offset, avnID, sizeof(avnID));
                offset += sizeof(avnID);
                memcpy(buffer + offset, flightNumber, sizeof(flightNumber));
                offset += sizeof(flightNumber);
            }

        void deserialize(const char* buffer, int& offset) 
            {
                memcpy(avnID, buffer + offset, sizeof(avnID));
                offset += sizeof(avnID);
                memcpy(flightNumber, buffer + offset, sizeof(flightNumber));
                offset += sizeof(flightNumber);
            }

        static int serializedSize() 
            {
                return sizeof(avnID) + sizeof(flightNumber);
            }
    };

class AVNGenerator          //class which generates avns and transfer their respective details to respective pipes
    {
        private:
            map<string, AVN> avnRecords;    //stores avns
            pthread_mutex_t avnMutex;       //toLock avnfuncitons during multithreading
            int fdAtcPipe = -1;             //for reading from atc_to_avn.fifo
            int fdStripePipe = -1;        //for writing to avn_to_stripe.fifo
            int fdAirPipe = -1;       //for writing to avn_to_airline.fifo
            int fdStripToAvnPipe = -1; //for reading payment confirmations from stripe_to_avn.fifo
            int fdNotifyAtcPipe = -1;    //for writing to avn_to_atc.fifo

        public:
            AVNGenerator() 
                {
                    pthread_mutex_init(&avnMutex, nullptr);
                    cout << "[AVN Generator] Starting initialization...\n" << flush;

                    //creating new fifos for communication with other files
                    if(mkfifo("avn_to_airline.fifo", 0666) == -1 && errno != EEXIST)
                        {
                            cout << "[ERROR] Failed to create avn_to_airline.fifo: " << strerror(errno) << endl << flush;
                            exit(1);
                        }
                    if(mkfifo("avn_to_stripe.fifo", 0666) == -1 && errno != EEXIST) 
                        {
                            cout << "[ERROR] Failed to create avn_to_stripe.fifo: " << strerror(errno) << endl << flush;
                            exit(1);
                        }
                    if(mkfifo("stripe_to_avn.fifo", 0666) == -1 && errno != EEXIST) 
                        {
                            cout << "[ERROR] Failed to create stripe_to_avn.fifo: " << strerror(errno) << endl << flush;
                            exit(1);
                        }

                    //opening atc_to_avn.fifo for reading
                    fdAtcPipe = open("atc_to_avn.fifo", O_RDONLY | O_NONBLOCK);
                    if(fdAtcPipe < 0) 
                        {
                            cout << "[ERROR] Failed to open atc_to_avn.fifo: " << strerror(errno) << endl << flush;
                            exit(1);
                        }
                    cout << "[AVN Generator] Successfully opened atc_to_avn.fifo\n" << flush;

                    //opening avn_to_stripe.fifo for writing
                    for(int attempt = 1; attempt <= 20; attempt++)      //ifStripe file not opened the nit will wait and retry 20 times until reader stripe.cpp opens
                        {
                            fdStripePipe = open("avn_to_stripe.fifo", O_WRONLY | O_NONBLOCK);
                            if(fdStripePipe < 0) {
                                cout << "[AVN Generator] Attempt " << attempt << " failed to open avn_to_stripe.fifo: " << strerror(errno) << ", retrying...\n" << flush;
                                usleep(500000);
                                continue;
                            }
                            cout << "[AVN Generator] Successfully opened avn_to_stripe.fifo\n" << flush;
                            break;
                        }
                    if(fdStripePipe < 0) 
                        {
                            cout << "[ERROR] Failed to open avn_to_stripe.fifo after retries: " << strerror(errno) << endl << flush;
                            close(fdAtcPipe);
                            exit(1);
                        }

                    //opengin avn_to_airline.fifo for writing
                    for(int attempt = 1; attempt <= 10; attempt++) 
                        {
                            fdAirPipe = open("avn_to_airline.fifo", O_WRONLY | O_NONBLOCK);
                            if(fdAirPipe < 0) {
                                cout << "[AVN Generator] Attempt " << attempt << " failed to open avn_to_airline.fifo: " << strerror(errno) << ", retrying...\n" << flush;
                                usleep(500000);
                                continue;
                            }
                            cout << "[AVN Generator] Successfully opened avn_to_airline.fifo\n" << flush;
                            break;
                        }
                    if(fdAirPipe < 0) 
                        {
                            cout << "[ERROR] Failed to open avn_to_airline.fifo after retries: " << strerror(errno) << endl << flush;
                            close(fdAtcPipe);
                            close(fdStripePipe);
                            exit(1);
                        }

                    //opening stripe_to_avn.fifo for reading payment confirmations
                    fdStripToAvnPipe = open("stripe_to_avn.fifo", O_RDONLY | O_NONBLOCK);
                    if(fdStripToAvnPipe < 0) 
                        {
                            cout << "[ERROR] Failed to open stripe_to_avn.fifo: " << strerror(errno) << endl << flush;
                            close(fdAtcPipe);
                            close(fdStripePipe);
                            close(fdAirPipe);
                            exit(1);
                        }
                    cout << "[AVN Generator] Successfully opened stripe_to_avn.fifo\n" << flush;

                    //opening avn_to_atc.fifo for writing
                    for(int attempt = 1; attempt <= 20; attempt++) 
                        {
                            fdNotifyAtcPipe = open("avn_to_atc.fifo", O_WRONLY | O_NONBLOCK);
                            if(fdNotifyAtcPipe < 0) 
                                {
                                    cout << "[AVN Generator] Attempt " << attempt << " failed to open avn_to_atc.fifo: " << strerror(errno) << ", retrying...\n" << flush;
                                    usleep(1500000); //3/2 secind delay
                                    continue;
                                }
                            cout << "[AVN Generator] Successfully opened avn_to_atc.fifo\n" << flush;
                            break;
                        }
                    if(fdNotifyAtcPipe < 0) 
                        {
                            cout << "[ERROR] Failed to open avn_to_atc.fifo after retries: " << strerror(errno) << endl << flush;
                            close(fdAtcPipe);
                            close(fdStripePipe);
                            close(fdAirPipe);
                            close(fdStripToAvnPipe);
                            exit(1);
                        }

                    // Send readiness signal to ATC via avn_ctrl.fifo
                    bool signalSent = false;
                    for(int attempt = 1; attempt <= 20; attempt++) 
                        {
                            int fd_ctrl = open("avn_ctrl.fifo", O_WRONLY | O_NONBLOCK);
                            if(fd_ctrl < 0) 
                                {
                                    cout << "[AVN Generator] Attempt " << attempt << " failed to open avn_ctrl.fifo for writing: " << strerror(errno) << ", retrying...\n" << flush;
                                    usleep(1500000); // 1.5-second delay
                                    continue;
                                }
                            const char* ready_msg = "AVN_READY";
                            int bytesWritten = write(fd_ctrl, ready_msg, strlen(ready_msg) + 1);
                            if(bytesWritten > 0) 
                                {
                                    cout << "[AVN Generator] Sent readiness signal to ATC (bytes written: " << bytesWritten << ")\n" << flush;
                                    signalSent = true;
                                    close(fd_ctrl);
                                    break;
                                } 
                            else 
                                {
                                    cout << "[AVN Generator] Attempt " << attempt << " failed to write readiness signal: " << strerror(errno) << ", retrying...\n" << flush;
                                    close(fd_ctrl);
                                    usleep(1500000); // 1.5-second delay
                                    continue;
                                }
                        }
                    if(!signalSent) 
                        {
                            cout << "[ERROR] Failed to send readiness signal to ATC after 20 attempts\n" << flush;
                            close(fdAtcPipe);
                            close(fdStripePipe);
                            close(fdAirPipe);
                            close(fdStripToAvnPipe);
                            close(fdNotifyAtcPipe);
                            exit(1);
                        }

                    cout << "[AVN Generator] Initialization complete.\n" << flush;
                }

            void forwardAVN(const AVN& avn, const string& pipeName, int fd) 
                {
                    char buffer[AVN::serializedSize()];
                    int offset = 0;
                    avn.serialize(buffer, offset);

                    int bytesWritten = write(fd, buffer, sizeof(buffer));
                    if(bytesWritten == sizeof(buffer)) 
                        {
                            cout << "[AVN Generator] Successfully forwarded AVN " << avn.avnID << " to " << pipeName << " (bytes: " << bytesWritten << ")\n" << flush;
                        } 
                    else 
                        {
                            cout << "[ERROR] Failed to forward AVN " << avn.avnID << " to " << pipeName << ", bytes written: " << bytesWritten << ", expected: " << sizeof(buffer) << ", error: " << strerror(errno) << endl << flush;
                        }
                }

            void notifyATCViolationCleared(const string& avnID, const string& flightNumber) 
                {
                    ViolationClearedNotification notification;
                    strncpy(notification.avnID, avnID.c_str(), sizeof(notification.avnID) - 1);
                    notification.avnID[sizeof(notification.avnID) - 1] = '\0';
                    strncpy(notification.flightNumber, flightNumber.c_str(), sizeof(notification.flightNumber) - 1);
                    notification.flightNumber[sizeof(notification.flightNumber) - 1] = '\0';

                    char buffer[ViolationClearedNotification::serializedSize()];
                    int offset = 0;
                    notification.serialize(buffer, offset);

                    int bytesWritten = write(fdNotifyAtcPipe, buffer, sizeof(buffer));
                    if(bytesWritten == sizeof(buffer)) 
                        {
                            cout << "[AVN Generator] Notified ATC that violation " << avnID << " for flight " << flightNumber << " has been cleared\n" << flush;
                        } 
                    else 
                        {
                            cout << "[ERROR] Failed to notify ATC for AVN " << avnID << ", bytes written: " << bytesWritten << ", expected: " << sizeof(buffer) << ", error: " << strerror(errno) << endl << flush;
                        }
                }

            void processAVN() 
                {
                    char buffer[AVN::serializedSize()];
                    int bytesRead = read(fdAtcPipe, buffer, sizeof(buffer));

                    if(bytesRead == sizeof(buffer)) 
                        {
                            AVN avn;
                            int offset = 0;
                            avn.deserialize(buffer, offset);

                            pthread_mutex_lock(&avnMutex);
                            avnRecords[avn.avnID] = avn;            //storing avn
                            pthread_mutex_unlock(&avnMutex);

                            cout << "[AVN Generator] Received AVN: " << avn.avnID << ", Flight: " << avn.flightNumber << ", Airline: " << avn.airlineName << ", Fine: PKR " << avn.fineAmount << endl << flush;

                            forwardAVN(avn, "avn_to_stripe.fifo", fdStripePipe);        //forwarding to StripePay

                            forwardAVN(avn, "avn_to_airline.fifo", fdAirPipe);          //forwarfing to airlinePortal
                        } 
                    else if(bytesRead < 0 && errno != EAGAIN) 
                        {
                            cout << "[ERROR] Failed to read from atc_to_avn.fifo: " << strerror(errno) << endl << flush;
                        }
                }

            void confirmPayment()       //to check confirmation of payment
                {
                    char buffer[PaymentConfirmation::serializedSize()];
                    int bytesRead = read(fdStripToAvnPipe, buffer, sizeof(buffer));     //reading data into buffer

                    if(bytesRead == sizeof(buffer)) 
                        {
                            PaymentConfirmation confirmation;
                            int offset = 0;
                            confirmation.deserialize(buffer, offset);           //converting back to class

                            if(confirmation.paymentSuccessful)              //if paymentDone
                                {
                                    pthread_mutex_lock(&avnMutex);
                                    auto i = avnRecords.find(confirmation.avnID);
                                    if(i != avnRecords.end()) 
                                        {
                                            AVN& avn = i->second;
                                            strncpy(avn.paymentStatus, "paid", sizeof(avn.paymentStatus) - 1);
                                            avn.paymentStatus[sizeof(avn.paymentStatus) - 1] = '\0';

                                            cout << "[AVN Generator] Updated payment status to 'paid' for AVN " << avn.avnID << ", Flight: " << avn.flightNumber << endl << flush;

                                            forwardAVN(avn, "avn_to_airline.fifo", fdAirPipe);      //updating status in airlineportal

                                            notifyATCViolationCleared(avn.avnID, avn.flightNumber); //updating status in atc
                                        } 
                                    else 
                                        {
                                            cout << "[ERROR] AVN " << confirmation.avnID << " not found in records\n" << flush;
                                        }
                                    pthread_mutex_unlock(&avnMutex);
                                } 
                            else 
                                {
                                cout << "[AVN Generator] Payment failed for AVN " << confirmation.avnID << ", Flight: " << confirmation.flightNumber << endl << flush;
                                }
                        } 
                    else if(bytesRead < 0 && errno != EAGAIN) 
                        {
                            cout << "[ERROR] Failed to read from stripe_to_avn.fifo: " << strerror(errno) << endl << flush;
                        }
                }

            void start() 
                {
                    while(true) 
                        {
                            processAVN();           //function to checkFor coming avns from atc

                            confirmPayment();       //function to confirm isPayment confirmed from StripePay

                            usleep(100000);         //blocking for 100ms
                        }
                }

            ~AVNGenerator() 
                {
                    if(fdAtcPipe >= 0) 
                        close(fdAtcPipe);
                    if(fdStripePipe >= 0) 
                        close(fdStripePipe);
                    if(fdAirPipe >= 0) 
                        close(fdAirPipe);
                    if(fdStripToAvnPipe >= 0) 
                        close(fdStripToAvnPipe);
                    if(fdNotifyAtcPipe >= 0) 
                        close(fdNotifyAtcPipe);
                    pthread_mutex_destroy(&avnMutex);
                }
    };

int main() 
    {
        cout << "===== AVN Generator Starting =====\n" << flush;
        AVNGenerator avnGen;
        avnGen.start();
        cout << "===== AVN Generator Complete =====\n" << flush;
        return 0;
    }
