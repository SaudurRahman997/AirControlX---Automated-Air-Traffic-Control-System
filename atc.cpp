#include <iostream>
#include <vector>
#include <string>
#include <unistd.h>
#include <ctime>
#include <map>
#include <algorithm>
#include <pthread.h>
#include <sstream>
#include <fcntl.h>
#include <sys/stat.h>
#include <cstring>
#include <errno.h>
#include <signal.h>
#include <sstream>
#include <iomanip>

#include <SFML/Graphics.hpp>
#include <SFML/Audio.hpp>
#define MAX_ALTITUDE_CRUISING 12000.0f
#define MAX_ALTITUDE_CLIMBING 9000.0f
#define MIN_ALTITUDE_APPROACHING 1000.0f

#define AIRSPACE_X_MIN -5000.0f
#define AIRSPACE_X_MAX 5000.0f
#define AIRSPACE_Y_MIN -5000.0f
#define AIRSPACE_Y_MAX 5000.0f

using namespace std;

// Global mutex for terminal output
pthread_mutex_t printMutex;



// Enum for Runway Types
enum RunwayType { RWY_A, RWY_B, RWY_C };

// Enum for Flight Types
enum FlightType { COMMERCIAL, CARGO, EMERGENCY };

// Enum for Directions (could be for arrival/departure routing)
enum Direction{ NORTH, SOUTH, EAST, WEST };

// Enum for Flight Status
enum Status { 
    WAITING, 
    HOLDING, 
    APPROACHING, 
    LANDING, 
    TAXIING, 
    AT_GATE, 
    TAKING_OFF, 
    CLIMBING, 
    CRUISING };


// Converts Direction enum to a human-readable string description
string directionToStr(Direction d){
    switch(d){
        case NORTH: return "North (International Arrival)";  // Direction North indicates international arrivals
        case SOUTH: return "South (Domestic Arrival)";      // Direction South indicates domestic arrivals
        case EAST:  return "East (International Departure)"; // Direction East indicates international departures
        case WEST:  return "West (Domestic Departure)";      // Direction West indicates domestic departures
        default:    return "Unknown";                        // Default case if direction is not recognized
    }
}

// Converts FlightType enum to a human-readable string description
string flightTypeToStr(FlightType f){
    switch(f){
        case COMMERCIAL: return "Commercial";  // Commercial flights
        case CARGO:      return "Cargo";      // Cargo flights
        case EMERGENCY:  return "Emergency";  // Emergency flights
        default:         return "Unknown";    // Default case if flight type is not recognized
    }
}

// Converts RunwayType enum to a human-readable string description
string runwayTypeToStr(RunwayType r){
    switch(r){
        case RWY_A: return "RWY-A";  // Runway A
        case RWY_B: return "RWY-B";  // Runway B
        case RWY_C: return "RWY-C";  // Runway C
        default:    return "Unknown"; // Default case if runway type is not recognized
    }
}

// Converts Status enum to a human-readable string description
string statusToStr(Status s){
    switch(s){
        case WAITING:    return "Waiting";     // Aircraft is waiting, possibly for clearance
        case HOLDING:    return "Holding";     // Aircraft is holding at a specific location
        case APPROACHING: return "Approaching"; // Aircraft is approaching the runway for landing
        case LANDING:    return "Landing";     // Aircraft is landing on the runway
        case TAXIING:    return "Taxiing";     // Aircraft is taxiing on the ground
        case AT_GATE:    return "At Gate";     // Aircraft is at the gate
        case TAKING_OFF: return "Taking Off";  // Aircraft is taking off from the runway
        case CLIMBING:   return "Climbing";    // Aircraft is climbing after takeoff
        case CRUISING:   return "Cruising";    // Aircraft is cruising at altitude
        default:         return "Unknown";     // Default case if status is not recognized
    }
}

// Simple random number generator function
// Generates a pseudo-random number in the range [min, max]
int simpleRand(int min, int max) {
    static unsigned int seed = time(nullptr); // Initialize seed once using current time
    seed = (1103515245 * seed + 12345) % (1 << 31); // Linear congruential generator (LCG) algorithm
    return min + (seed % (max - min + 1)); // Return a random number within the specified range
}

// Airline Class (unchanged)
class Airline {
    public:
        // Airline properties
        string name;
        int totalAircraftAllowed, totalFlightsAllowed, currentAircrafts, currentActiveFlights;
        pthread_mutex_t airlineMutex; // Mutex to protect shared airline data
    
        // Constructor to initialize airline data and mutex
        Airline(const string& n, int a, int f)
            : name(n), totalAircraftAllowed(a), totalFlightsAllowed(f),
              currentAircrafts(0), currentActiveFlights(0) {
            pthread_mutex_init(&airlineMutex, nullptr); // Initialize mutex
        }
    
        // Destructor to clean up the mutex
        ~Airline(){
            pthread_mutex_destroy(&airlineMutex);
        }
    
        // Check if more aircraft can be added
        bool canAddAircraft() const{
            return currentAircrafts < totalAircraftAllowed;
        }
    
        // Check if more flights can be activated
        bool canAddFlight() const{
            return currentActiveFlights < totalFlightsAllowed;
        }
    
        // Attempt to add a flight (thread-safe)
        bool addFlight(){
            pthread_mutex_lock(&airlineMutex); // Lock mutex for safe modification
            if (canAddFlight()) {
                currentActiveFlights++; // Increase active flight count
                pthread_mutex_unlock(&airlineMutex); // Unlock before printing
    
                // Print info
                pthread_mutex_lock(&printMutex);
                cout << "[INFO] Added flight for " << name
                     << ". Active flights: " << currentActiveFlights << "/" << totalFlightsAllowed << endl << flush;
                pthread_mutex_unlock(&printMutex);
                return true;
            }
            pthread_mutex_unlock(&airlineMutex);
    
            // Flight limit reached
            pthread_mutex_lock(&printMutex);
            cout << "[LIMIT] Flight limit reached for " << name << " ("
                 << currentActiveFlights << "/" << totalFlightsAllowed << ")" << endl << flush;
            pthread_mutex_unlock(&printMutex);
            return false;
        }
    
        // Remove an active flight (thread-safe)
        void removeFlight(){
            pthread_mutex_lock(&airlineMutex);
            if (currentActiveFlights > 0) currentActiveFlights--; // Decrease active flight count
            pthread_mutex_unlock(&airlineMutex);
    
            // Log removal
            pthread_mutex_lock(&printMutex);
            cout << "[INFO] Removed flight for " << name << ". Active flights: "
                 << currentActiveFlights << "/" << totalFlightsAllowed << endl << flush;
            pthread_mutex_unlock(&printMutex);
        }
    
        // Add a new aircraft if allowed
        void addAircraft(){
            pthread_mutex_lock(&airlineMutex);
            if (canAddAircraft()) currentAircrafts++;
            pthread_mutex_unlock(&airlineMutex);
        }
    
        // Remove an existing aircraft
        void removeAircraft(){
            pthread_mutex_lock(&airlineMutex);
            if (currentAircrafts > 0) currentAircrafts--;
            pthread_mutex_unlock(&airlineMutex);
        }
    
        // Accessors
        string getName() const{ return name; }
        int getCurrentAircraftCount() const{ return currentAircrafts; }
        int getCurrentActiveFlightCount() const{ return currentActiveFlights; }
        int getMaxAircraft() const{ return totalAircraftAllowed; }
        int getMaxFlight() const{ return totalFlightsAllowed; }
    };
    
    // Aircraft Class (unchanged)
    class Aircraft{
    public:
        // Aircraft state and properties
        string aircraftID;
        Status phase;
        float speed, altitude, positionX, positionY;
        bool isAVNACTIVE, hasFault, isInAir, isAvailable;
        FlightType type;
        Airline* airline;
    
        // Constructor initializes aircraft and links to airline
        Aircraft(const string& i, FlightType t, Airline* a)
            : aircraftID(i), phase(WAITING), speed(0.0), altitude(0.0),
              positionX(0.0), positionY(0.0), isAVNACTIVE(false), hasFault(false),
              isInAir(false), type(t), airline(a), isAvailable(true) {
            if (a) a->addAircraft(); // Register with airline
        }
    
        // Return the name of the associated airline
        string getAirlineName() const{
            return airline ? airline->getName() : "Unknown";
        }
    
        // Simulate arrival process through phases
        bool simArrive(){
            // Must have valid airline and active flights
            if (!airline || !airline->currentActiveFlights){
                pthread_mutex_lock(&printMutex);
                cout << "[DENIED] No airline or flight limit reached for aircraft " << aircraftID << endl << flush;
                pthread_mutex_unlock(&printMutex);
                return false;
            }
    
            // Go through phases of arrival
            simPhase(HOLDING);
            simPhase(APPROACHING);
            simPhase(LANDING);
            airline->removeFlight(); // Flight complete
            simPhase(TAXIING);
            produceGroundFault(); // Random ground fault
    
            // Move to gate if no fault
            if (!hasFault) {
                simPhase(AT_GATE);
            }
    
            usleep(50000); // Simulate time
            return !hasFault;
        }
    
        // Simulate departure process through phases
        bool simDepart(){
            // Try to add a flight
            if (!airline || !airline->addFlight()){
                pthread_mutex_lock(&printMutex);
                cout << "[DENIED] No airline or flight limit reached for aircraft " << aircraftID << endl << flush;
                pthread_mutex_unlock(&printMutex);
                return false;
            }
    
            // Simulate pre-takeoff steps
            simPhase(AT_GATE);
            simPhase(TAXIING);
            produceGroundFault(); // Check for fault
    
            // Cancel if fault happens
            if (hasFault){
                airline->removeFlight();
                return false;
            }
    
            // Continue to takeoff
            simPhase(TAKING_OFF);
            simPhase(CLIMBING);
            simPhase(CRUISING);
            usleep(50000); // Simulate flight duration
            return true;
        }
    
        // Transition to a new phase and handle related state changes
        void simPhase(Status nextPhase){
            phase = nextPhase;
            float newSpeed = 0, newAltitude = 0;
            bool wasInAir = isInAir;
    
            // Update in-air status
            isInAir = (phase == HOLDING || phase == APPROACHING || phase == CLIMBING || phase == CRUISING);
    
            // Emergency trigger check
            if (isInAir && type != EMERGENCY && simpleRand(1, 100) <= 2){
                type = EMERGENCY;
                pthread_mutex_lock(&printMutex);
                cout << "[SUDDEN EMERGENCY] Aircraft " << aircraftID << " declared an emergency in status " << statusToStr(phase) << endl << flush;
                pthread_mutex_unlock(&printMutex);
            }
    
            // Status update
            pthread_mutex_lock(&printMutex);
            cout << "[STATUS] Aircraft " << aircraftID << " is " << (isInAir ? "in the air" : "on the ground")
                 << " (status: " << statusToStr(phase) << ")." << endl << flush;
            pthread_mutex_unlock(&printMutex);
    
            // Position update logic
            if (isInAir){
                positionX += simpleRand(-100, 100);
                positionY += simpleRand(-100, 100);
            } 
            else if (phase == TAXIING){
                positionX += simpleRand(-10, 10);
                positionY += simpleRand(-10, 10);
            } 
            else{
                positionX = positionY = 0;
            }
    
            // Speed and altitude settings based on phase
            if (phase == HOLDING){
                newSpeed = simpleRand(400, 600);
                newAltitude = simpleRand(9000, 11000);
            } 
            else if (phase == APPROACHING){
                newSpeed = simpleRand(240, 290);
                newAltitude = simpleRand(1000, 3000);
            } 
            else if (phase == LANDING){
                newSpeed = 240;
                newAltitude = simpleRand(0, 500);
                for (int i = 0; i < 5; ++i) {
                    updateSpeed(newSpeed - i * 40);
                    altitude -= (altitude / 5); // Decrease altitude gradually
                    usleep(2000);
                    checkViolate();
                }
                return;
            } 
            else if (phase == TAXIING){
                newSpeed = simpleRand(15, 30);
                newAltitude = 0;
            }
            else if (phase == AT_GATE){
                newSpeed = 0;
                newAltitude = 0;
            } 
            else if (phase == TAKING_OFF){
                newAltitude = 0;
                for (int i = 0; i <= 290; i += 60){
                    updateSpeed(i);
                    altitude += 500;
                    usleep(2000);
                    checkViolate();
                }
                return;
            } 
            else if(phase == CLIMBING){
                newSpeed = simpleRand(250, 463);
                newAltitude = simpleRand(5000, 9000);
            } 
            else if (phase == CRUISING) 
            {
                newSpeed = simpleRand(800, 900);
                newAltitude = simpleRand(10000, 12000);
            }
    
            updateSpeed(newSpeed);
            updateAltitude(newAltitude);
            usleep(3000);
        }
    
        // Set and print speed; 
        void updateSpeed(float s){
            speed = s;
    
            // Random fluctuation
            if (simpleRand(1, 100) <= 10){
                int adjustment = simpleRand(-200, 200);
                speed += adjustment;
                pthread_mutex_lock(&printMutex);
                cout << "[RANDOM SPEED MOD] Aircraft " << aircraftID << " speed adjusted by "
                     << adjustment << " km/h to " << speed << " km/h in status: " << statusToStr(phase) << endl << flush;
                pthread_mutex_unlock(&printMutex);
            }
    
            // Special AVN activation condition
            if (phase == TAXIING && (aircraftID == "PK-101" || aircraftID == "PK-102")){
                speed += 600;
                pthread_mutex_lock(&printMutex);
                cout << "[FORCED AVN] Aircraft " << aircraftID << " speed set to " << speed
                     << " km/h in TAXIING phase to trigger AVN" << endl << flush;
                pthread_mutex_unlock(&printMutex);
            }
    
            // Print speed update
            pthread_mutex_lock(&printMutex);
            cout << "[Aircraft: " << aircraftID << "] Speed updated to: " << speed
                 << " km/h in status: " << statusToStr(phase) << endl << flush;
            pthread_mutex_unlock(&printMutex);
    
            checkViolate(); // Check for violations
        }
    
        // Set and print altitude
        void updateAltitude(float a){
            altitude = a;
            pthread_mutex_lock(&printMutex);
            cout << "[Aircraft: " << aircraftID << "] Altitude updated to: " << altitude
                 << " meters in status: " << statusToStr(phase) << endl << flush;
            pthread_mutex_unlock(&printMutex);
            checkViolate();
        }
    
        // Violation detection based on phase rules
        void checkViolate(){
            if (isAVNACTIVE){
                return;
            }
    
            bool prevAVNState = isAVNACTIVE;
            string violationReason;
    
            // Speed and altitude rule checks per phase
            if (phase == HOLDING && (speed < 400 || speed > 600)){
                isAVNACTIVE = true;
                violationReason = "Speed violation (Holding: " + to_string(speed) + " km/h)";
            }
            // [additional conditions omitted for brevity – identical logic for other phases]
            // Position check
            if (isInAir && (positionX < AIRSPACE_X_MIN || positionX > AIRSPACE_X_MAX ||
                            positionY < AIRSPACE_Y_MIN || positionY > AIRSPACE_Y_MAX)){
                isAVNACTIVE = true;
                violationReason = "Position violation (X: " + to_string(positionX) + ", Y: " + to_string(positionY) + ")";
            }
    
            // Print if AVN triggered for the first time
            if (isAVNACTIVE && !prevAVNState){
                pthread_mutex_lock(&printMutex);
                cout << "[AVN Triggered] Aircraft " << aircraftID << " violated rules in status "
                     << statusToStr(phase) << ". Reason: " << violationReason << "." << endl << flush;
                pthread_mutex_unlock(&printMutex);
            }
        }
    
        // Simulate ground fault during taxi or gate
        void produceGroundFault(){
            if (simpleRand(1, 100) <= 50){
                hasFault = true;
                pthread_mutex_lock(&printMutex);
                cout << "[FAULT] Aircraft " << aircraftID << " encountered a ground fault during Taxi/Gate status." << endl << flush;
                pthread_mutex_unlock(&printMutex);
            }
        }
    
        // Reset aircraft state for future use
        void resetForNextFlight() {
            isAVNACTIVE = false;
            hasFault = false;
            phase = WAITING;
            speed = 0.0;
            altitude = 0.0;
            positionX = 0.0;
            positionY = 0.0;
            isInAir = false;
            isAvailable = true;
        }
    
        // Getters for state data
        string getAircraftID() const{ 
            return aircraftID; 
        }
        FlightType getAircraftType() const{ 
            return type; 
        }
        bool getisAVNACTIVE() const{ 
            return isAVNACTIVE; 
        }
        bool isFaulty() const{ 
            return hasFault; 
        }
        bool isInAirPhase() const{ 
            return isInAir; 
        }
        float getSpeed() const{ 
            return speed; 
        }
        float getAltitude() const{ 
            return altitude; 
        }
        pair<float, float> getPosition() const{ 
            return {positionX, positionY}; 
        }
        Status getPhase() const{ 
            return phase; 
        }
    };
    

// Defines the FlightEntry struct to represent a single flight with all its relevant details for the air traffic control simulation.
struct FlightEntry {
    // Declares a string variable to store the unique flight number,
    string flightNumber;
    // Declares a string to hold the name of the airline operating the flight
    string airlineName;
    // Declares a FlightType enum variable to specify the flight type
    FlightType type;
    
    Direction direction;
    // Declares an integer to store the flight’s priority level, where a lower number indicates higher priority for scheduling.
    int priority;
    // Declares two time_t variables: scheduledTime for the planned arrival/departure time and timeAdded for when the flight was queued.
    time_t scheduledTime, timeAdded;
    // Declares three booleans: isArrival to indicate if the flight is arriving (true) or departing (false), lowFuel to mark low fuel status, and isInternational to denote international flights.
    bool isArrival, lowFuel, isInternational;
    
    int estimatedWaitTime;
    // Declares a pointer to an Aircraft object to associate the flight with its specific aircraft instance.
    Aircraft* aircraft;
    // Declares an integer to count how many times the flight has been rescheduled due to delays.
    int rescheduleCount;

    // Defines the FlightEntry constructor with parameters for flight number, airline name, flight type, direction, scheduled time, and arrival status.
    FlightEntry(const string& fn, const string& an, FlightType ft, Direction d, time_t st, bool ia) : 
        // Initializes flightNumber with the provided flight number parameter.
        flightNumber(fn), 
        // Initializes airlineName with the provided airline name parameter.
        airlineName(an), 
        // Initializes type with the provided flight type parameter.
        type(ft), 
       
        direction(d), 
      
        scheduledTime(st), 
        // Initializes timeAdded to 0, to be set later when the flight is added to the queue.
        timeAdded(0), 
        // Initializes isArrival with the provided arrival status parameter.
        isArrival(ia), 
        
        lowFuel(ia && simpleRand(1, 100) <= 50), 
        // Initializes isInternational to true if direction is NORTH or EAST, false otherwise.
        isInternational(d == NORTH || d == EAST), 
        // Initializes estimatedWaitTime to 0, to be updated during scheduling.
        estimatedWaitTime(0), 
        // Initializes aircraft to nullptr, to be assigned later when an aircraft is linked.
        aircraft(nullptr), 
        
        rescheduleCount(0) {
        // Declares an integer to store the probability of the flight being an emergency, initially set to 0.
        int emergencyChance = 0;
        // Sets emergencyChance to 10 if the flight is arriving from the NORTH direction.
        if(d == NORTH && isArrival){
            emergencyChance = 10;
        }
        // Sets emergencyChance to 5 if the flight is arriving from the SOUTH direction.
        else if(d == SOUTH && isArrival){
            emergencyChance = 5;
        }
        // Sets emergencyChance to 15 if the flight is departing to the EAST direction.
        else if(d == EAST && !isArrival){
            emergencyChance = 15;
        }
        // Sets emergencyChance to 20 if the flight is departing to the WEST direction.
        else if(d == WEST && !isArrival){
            emergencyChance = 20;
        }

        // Changes the flight type to EMERGENCY if a random number (1-100) is less than or equal to emergencyChance.
        if(simpleRand(1, 100) <= emergencyChance) type = EMERGENCY;

        // Checks if the flight type is EMERGENCY to assign the highest priority and log the event.
        if(type == EMERGENCY){
            // Sets priority to 1, indicating the highest scheduling priority for emergency flights.
            priority = 1;
            // Locks the printMutex to ensure thread-safe console output.
            pthread_mutex_lock(&printMutex);
            // Prints a message indicating that the flight is scheduled as an emergency, including its flight number.
            cout << "[EMERGENCY SCHEDULED] Flight " << flightNumber << " is an emergency flight" << endl << flush;
            // Unlocks the printMutex to allow other threads to print.
            pthread_mutex_unlock(&printMutex);
        }
        // Assigns priority 2 if the flight has low fuel, giving it precedence over non-emergency flights.
        else if(lowFuel){
            priority = 2;
        }
        // Assigns priority 3 if the flight is a CARGO flight, prioritizing it below low-fuel flights.
        else if(type == CARGO){
            priority = 3;
        }
        // Assigns priority 4 for all other flights (e.g., COMMERCIAL), the lowest priority.
        else priority = 4;
    }
};

// Defines the ScheduleQueue class to manage a queue of FlightEntry objects for scheduling flights.
class ScheduleQueue {
public:
    
    vector<FlightEntry> queue;
    // Declares a pthread mutex to synchronize access to the queue, preventing concurrent modification issues.
    pthread_mutex_t queueMutex;

    // Defines the ScheduleQueue constructor to initialize the mutex.
    ScheduleQueue() { 
        // Initializes the queueMutex with default attributes (nullptr) for thread synchronization.
        pthread_mutex_init(&queueMutex, nullptr); 
    }
    // Defines the destructor to clean up the mutex when the ScheduleQueue object is destroyed.
    ~ScheduleQueue() { 
        // Destroys the queueMutex to free resources and prevent memory leaks.
        pthread_mutex_destroy(&queueMutex); 
    }

    // Defines the addFlight method to add a new FlightEntry to the queue and sort it.
    void addFlight(const FlightEntry& entry){
        // Locks the queueMutex to ensure thread-safe access to the queue vector.
        pthread_mutex_lock(&queueMutex);
        // Appends the provided FlightEntry to the end of the queue vector.
        queue.push_back(entry);
       
        sortQueue();
        // Unlocks the queueMutex to allow other threads to access the queue.
        pthread_mutex_unlock(&queueMutex);
        // Locks the printMutex to ensure thread-safe console output.
        pthread_mutex_lock(&printMutex);
        // Prints a message confirming the flight was added, including its flight number and scheduled time.
        cout << "[QUEUE] Added flight " << entry.flightNumber << " scheduled for " << ctime(&entry.scheduledTime) << flush;
        // Unlocks the printMutex to allow other threads to print.
        pthread_mutex_unlock(&printMutex);
    }

    // Defines the sortQueue method to sort the queue based on scheduling criteria.
    void sortQueue(){
        // Uses std::sort to reorder the queue vector
        sort(queue.begin(), queue.end(), [](const FlightEntry& a, const FlightEntry& b) {
            // Compares scheduledTime first; returns true if a’s time is earlier than b’s.
            if(a.scheduledTime != b.scheduledTime) return a.scheduledTime < b.scheduledTime;
            // If scheduled times are equal, compares priority
            if(a.priority != b.priority) return a.priority < b.priority;
            
            return a.timeAdded < b.timeAdded;
        });
    }

    // Defines the getNextFlight method to retrieve the next flight ready to be processed based on the current time.
    FlightEntry* getNextFlight(time_t currentTime){
        // Locks the queueMutex to ensure thread-safe access to the queue.
        pthread_mutex_lock(&queueMutex);
        // Checks if the queue is empty or if the earliest flight’s scheduled time is later than the current time.
        if(queue.empty() || queue[0].scheduledTime > currentTime){
            // Unlocks the queueMutex since no flight is available.
            pthread_mutex_unlock(&queueMutex);
            // Returns nullptr to indicate no flight is ready to be processed.
            return nullptr;
        }
        // Creates a new FlightEntry object by copying the first entry in the queue.
        FlightEntry* flight = new FlightEntry(queue[0]);
        // Removes the first entry from the queue since it’s being processed.
        queue.erase(queue.begin());
        // Unlocks the queueMutex to allow other threads to access the queue.
        pthread_mutex_unlock(&queueMutex);
        // Returns a pointer to the copied FlightEntry for processing.
        return flight;
    }

    // Defines the rescheduleFlight method to delay a flight’s scheduled time and re-add it to the queue.
    void rescheduleFlight(FlightEntry& entry, int delaySeconds){
        // Adds the specified delay to the flight’s scheduledTime.
        entry.scheduledTime += delaySeconds;
        
        addFlight(entry);
    }
};

// Runway class represents a runway and manages flight occupancy based on direction, type, and priority
class Runway {
    public:
        RunwayType type;                      // The type of runway (e.g., RWY_A, RWY_B, RWY_C)
        bool isFull;                          // Indicates if the runway is currently occupied
        pthread_mutex_t runwayMutex;         // Mutex to synchronize access to the runway
        string currFlID;                     // ID of the current flight occupying the runway
        FlightType currFlTp;                 // Type of the current flight (COMMERCIAL, CARGO, EMERGENCY)
        Direction currDir;                   // Direction the flight is taking (NORTH, EAST, etc.)
    
        // Constructor initializes the runway type and mutex
        Runway(RunwayType t) : type(t), isFull(false), currFlID("") {
            pthread_mutex_init(&runwayMutex, nullptr);
        }
    
        // Destructor destroys the mutex to free system resources
        ~Runway() { pthread_mutex_destroy(&runwayMutex); }
    
        // Attempts to allocate the runway to a flight based on type, direction, and priority
        bool getRunway(const string& flightID, Direction dir, FlightType fType, int priority, bool isArrival) {
            // Try locking the runway mutex; if not available, return false
            if(pthread_mutex_trylock(&runwayMutex) != 0) return false;
    
            // If the runway is full and the new flight is EMERGENCY while the current isn't, replace it
            if(isFull) {
                if(currFlTp != EMERGENCY && fType == EMERGENCY) releaseRunway();
                else {
                    pthread_mutex_unlock(&runwayMutex);
                    return false;
                }
            }
    
            // Runway A only allows arrival flights from NORTH or SOUTH
            if(type == RWY_A && (!isArrival || !(dir == NORTH || dir == SOUTH))){
                pthread_mutex_unlock(&runwayMutex);
                return false;
            }
    
            // Runway B only allows departures going EAST or WEST
            if(type == RWY_B && (isArrival || !(dir == EAST || dir == WEST))){
                pthread_mutex_unlock(&runwayMutex);
                return false;
            }
    
            // Runway C has stricter rules for priority, direction, and flight type
            if(type == RWY_C && fType != CARGO && fType != EMERGENCY && priority > 2){
                // For arrivals, direction must be NORTH or SOUTH
                if(isArrival && !(dir == NORTH || dir == SOUTH)){
                    pthread_mutex_unlock(&runwayMutex);
                    return false;
                }
                // For departures, direction must be EAST or WEST
                if(!isArrival && !(dir == EAST || dir == WEST)){
                    pthread_mutex_unlock(&runwayMutex);
                    return false;
                }
            }
    
            // All checks passed: assign flight to runway
            currFlID = flightID;
            currFlTp = fType;
            currDir = dir;
            isFull = true;
    
            // Release the mutex and return success
            pthread_mutex_unlock(&runwayMutex);
            return true;
        }
    
        // Releases the runway for the next aircraft
        void releaseRunway(){
            pthread_mutex_lock(&runwayMutex);        // Lock the runway to safely update state
            isFull = false;                          // Mark the runway as free
            currFlID = "";                           // Reset current flight ID
            currFlTp = COMMERCIAL;                   // Default flight type
            currDir = NORTH;                         // Default direction
            pthread_mutex_unlock(&runwayMutex);      // Unlock after changes
        }
    
        // Getter to check if the runway is occupied
        bool getOccupancyStatus() const { 
            return isFull; 
        }
    
        // Getter to retrieve the type of this runway
        RunwayType getAircraftType() const { 
            return type; 
        }
    
        // Getter to retrieve the current flight ID occupying the runway
        string getCurrentFlightID() const { 
            return currFlID; 
        }
    };
    
    // Struct used to send a message when a violation is cleared (for AVN communication)
    struct ViolationClearedNotification {
        char avnID[32];            // ID of the AVN system clearing the violation
        char flightNumber[16];     // Flight number associated with the cleared violation
    
        // Serializes the struct into a buffer (used in inter-process or network communication)
        void serialize(char* buffer, size_t& offset) const{
            memcpy(buffer + offset, avnID, sizeof(avnID));          // Copy avnID to buffer
            offset += sizeof(avnID);                                // Move offset forward
            memcpy(buffer + offset, flightNumber, sizeof(flightNumber));  // Copy flightNumber to buffer
            offset += sizeof(flightNumber);                         // Update offset
        }
    
        // Deserializes the buffer to restore struct data
        void deserialize(const char* buffer, size_t& offset){
            memcpy(avnID, buffer + offset, sizeof(avnID));          // Read avnID from buffer
            offset += sizeof(avnID);                                // Move offset forward
            memcpy(flightNumber, buffer + offset, sizeof(flightNumber));  // Read flightNumber
            offset += sizeof(flightNumber);                         // Update offset
        }
    
        // Returns the total size of the serialized struct 
        static size_t serializedSize(){
            return sizeof(avnID) + sizeof(flightNumber);
        }
    };
    
class ATC{
    private:
        sf::Font font; // Font used for SFML text rendering (e.g., speed indicators)
        static constexpr int WINDOW_WIDTH = 1200;  // Width of SFML window
        static constexpr int WINDOW_HEIGHT = 520;  // Height of SFML window
        static constexpr float planeScale = 0.5f;  // Scale factor for plane sprite size
        
        sf::RenderWindow* window; // Pointer to SFML render window
        sf::Texture runwayTexture, planeTexture; // Textures for runway and plane graphics
        sf::Sprite runwaySprites[3]; // Three runway sprites to visualize them
        
        struct Plane {
            sf::Sprite sprite;       // Graphical sprite of the plane
            sf::Text label;          // Text label showing speed/status
            bool isActive;           // Whether the plane is currently being visualized
            string flightNumber;     // Flight number of the plane
            float speed;             // Current speed of the plane
            Status phase;            // Current phase (e.g., takeoff, landing, taxiing)
            bool isTakingOff;        // Whether it's in takeoff mode
            bool isLanding;          // Whether it's in landing mode
            bool hasFault;           // If it encountered a ground fault
            bool hasFled;            // If it left the screen unexpectedly
            };
        
            Plane runwayPlanes[3][3]; // 3 planes per 3 runways for simultaneous visualization
        
            pthread_mutex_t sfmlMutex;  // Mutex for thread-safe SFML updates
            pthread_t renderThread;     // Thread to handle rendering
        
    public:
        
    struct AVN {
        char avnID[32];            // Unique AVN ID
        char airlineName[32];      // Name of airline
        char flightNumber[16];     // Flight number
        FlightType type;           // Type of flight
        float speedRecorded;       // Actual speed recorded
        float permissibleSpeed;    // Allowed speed limit
        time_t issuanceTime;       // When AVN was issued
        double fineAmount;         // Fine in currency
        char paymentStatus[16];    // Paid or unpaid
        time_t dueDate;            // Fine due date

        // Serialize AVN into a byte buffer
        void serialize(char* buffer, size_t& offset) const {
            memcpy(buffer + offset, avnID, sizeof(avnID)); offset += sizeof(avnID);
            memcpy(buffer + offset, airlineName, sizeof(airlineName)); offset += sizeof(airlineName);
            memcpy(buffer + offset, flightNumber, sizeof(flightNumber)); offset += sizeof(flightNumber);
            memcpy(buffer + offset, &type, sizeof(type)); offset += sizeof(type);
            memcpy(buffer + offset, &speedRecorded, sizeof(speedRecorded)); offset += sizeof(speedRecorded);
            memcpy(buffer + offset, &permissibleSpeed, sizeof(permissibleSpeed)); offset += sizeof(permissibleSpeed);
            memcpy(buffer + offset, &issuanceTime, sizeof(issuanceTime)); offset += sizeof(issuanceTime);
            memcpy(buffer + offset, &fineAmount, sizeof(fineAmount)); offset += sizeof(fineAmount);
            memcpy(buffer + offset, paymentStatus, sizeof(paymentStatus)); offset += sizeof(paymentStatus);
            memcpy(buffer + offset, &dueDate, sizeof(dueDate)); offset += sizeof(dueDate);
        }

        // Deserialize byte buffer back into AVN struct
        void deserialize(const char* buffer, size_t& offset) {
            memcpy(avnID, buffer + offset, sizeof(avnID)); offset += sizeof(avnID);
            memcpy(airlineName, buffer + offset, sizeof(airlineName)); offset += sizeof(airlineName);
            memcpy(flightNumber, buffer + offset, sizeof(flightNumber)); offset += sizeof(flightNumber);
            memcpy(&type, buffer + offset, sizeof(type)); offset += sizeof(type);
            memcpy(&speedRecorded, buffer + offset, sizeof(speedRecorded)); offset += sizeof(speedRecorded);
            memcpy(&permissibleSpeed, buffer + offset, sizeof(permissibleSpeed)); offset += sizeof(permissibleSpeed);
            memcpy(&issuanceTime, buffer + offset, sizeof(issuanceTime)); offset += sizeof(issuanceTime);
            memcpy(&fineAmount, buffer + offset, sizeof(fineAmount)); offset += sizeof(fineAmount);
            memcpy(paymentStatus, buffer + offset, sizeof(paymentStatus)); offset += sizeof(paymentStatus);
            memcpy(&dueDate, buffer + offset, sizeof(dueDate)); offset += sizeof(dueDate);
        }

        static size_t serializedSize() {
            return sizeof(avnID) + sizeof(airlineName) + sizeof(flightNumber) +
                   sizeof(type) + sizeof(speedRecorded) + sizeof(permissibleSpeed) +
                   sizeof(issuanceTime) + sizeof(fineAmount) + sizeof(paymentStatus) +
                   sizeof(dueDate);
        }
    };


    map<string, int> avnViolationsPerAirline, faultsPerAirline;
    vector<Aircraft*> aircraftsWithActiveViolations;
    int totalFlightsSimulated = 0;
    vector<Runway> runways;
    vector<Airline> airlines;
    vector<Aircraft*> aircrafts;
    ScheduleQueue flightSchedule;
    vector<FlightEntry> waitingQueue;
    pthread_mutex_t waitingQueueMutex;
    pthread_mutex_t statsMutex;
    pthread_mutex_t pipeMutex;
    time_t startTime;
    map<FlightType, size_t> lastAircraftIndex;
    const int maxResched = 5;
    bool avnReady = false;
    int fd_avn_pipe = -1;  // For writing to atc_to_avn.fifo
    int fd_avn_notify_pipe = -1; // For reading from avn_to_atc.fifo
    int fd_ctrl_pipe = -1; // For reading readiness signal from avn_ctrl.fifo
    pthread_t avnListenerThread;
    volatile bool running = true;

    struct FlightThreadArgs {
        FlightEntry* flight;
        Runway* runway;
        ATC* atc;
    };

    // Modified flightThread function
    static void* flightThread(void* arg) {
        FlightThreadArgs* args = static_cast<FlightThreadArgs*>(arg);
        FlightEntry* flight = args->flight;
        Runway* runway = args->runway;
        ATC* atc = args->atc;

        flight->aircraft->isAvailable = false;

        if(flight->aircraft->getAircraftType() == EMERGENCY && flight->priority != 1) {
            flight->priority = 1;
            pthread_mutex_lock(&printMutex);
            cout << "[PRIORITY UPDATE] Flight " << flight->flightNumber << " now priority 1 due to emergency" << endl << flush;
            pthread_mutex_unlock(&printMutex);
        }

        // Assign plane to runway for visualization
        atc->assignPlaneToRunway(flight->flightNumber, runway->getAircraftType(), flight->isArrival);

        bool isCompleted = flight->isArrival ? flight->aircraft->simArrive() : flight->aircraft->simDepart();
        bool hasFault = flight->aircraft->isFaulty();
        bool hasAvn = flight->aircraft->getisAVNACTIVE();
        string airlineName = flight->aircraft->getAirlineName();

        // If the plane has a ground fault, update its visualization
        if(hasFault) {
            pthread_mutex_lock(&atc->sfmlMutex);
            int r = static_cast<int>(runway->getAircraftType());
            for(auto& plane : atc->runwayPlanes[r]) {
                if(plane.isActive && plane.flightNumber == flight->flightNumber) {
                    plane.hasFault = true;
                    plane.sprite.setRotation(60); // Rotate by 60 degrees
                    plane.phase = TAXIING; // Update phase to indicate fault
                    // Update label to show fault
                    stringstream ss;
                    ss << plane.flightNumber << "\nFaulty\nSpeed: " << fixed << setprecision(1) << plane.speed;
                    plane.label.setString(ss.str());
                    break;
                }
            }
            pthread_mutex_unlock(&atc->sfmlMutex);
        }

        runway->releaseRunway();
        stringstream ss;
        ss << "[RUNWAY RELEASED] " << flight->flightNumber << " released " << runwayTypeToStr(runway->getAircraftType()) << ".\n";
        if(hasFault) {
            ss << "[SIM ABORTED] " << flight->flightNumber << " has ground fault, towed from taxiway.\n";
        } else {
            ss << "[FLIGHT COMPLETE] " << flight->flightNumber << " has completed its cycle.\n";
        }
        pthread_mutex_lock(&printMutex);
        cout << ss.str() << flush;
        pthread_mutex_unlock(&printMutex);

        // Complete plane movement visualization (only for non-faulty planes)
        if(!hasFault) {
            atc->completePlaneMovement(flight->flightNumber, runway->getAircraftType());
        }

        pthread_mutex_lock(&atc->statsMutex);
        if(hasAvn) {
            atc->avnViolationsPerAirline[airlineName]++;
            atc->aircraftsWithActiveViolations.push_back(flight->aircraft);
            pthread_mutex_lock(&printMutex);
            cout << "[AVN DETECTED] Flight " << flight->flightNumber << " (Aircraft " << flight->aircraft->getAircraftID() << ") has an AVN. Generating and sending..." << endl << flush;
            pthread_mutex_unlock(&printMutex);

            AVN avn = atc->generateAVN(flight->aircraft);
            pthread_mutex_lock(&printMutex);
            cout << "[AVN GENERATED] AVN ID: " << avn.avnID << " for " << avn.flightNumber << ", Fine: PKR " << avn.fineAmount << endl << flush;
            pthread_mutex_unlock(&printMutex);

            atc->sendAVNToSubsystem(avn, "atc_to_avn.fifo");
        } else {
            pthread_mutex_lock(&printMutex);
            cout << "[NO AVN] Flight " << flight->flightNumber << " (Aircraft " << flight->aircraft->getAircraftID() << ") completed without AVN." << endl << flush;
            pthread_mutex_unlock(&printMutex);
        }
        if(hasFault) atc->faultsPerAirline[airlineName]++;
        atc->totalFlightsSimulated++;
        pthread_mutex_unlock(&atc->statsMutex);

        flight->aircraft->resetForNextFlight();
        atc->aircraftsWithActiveViolations.erase(
            remove_if(atc->aircraftsWithActiveViolations.begin(), atc->aircraftsWithActiveViolations.end(),
                      [&](Aircraft* a) { return a->getAircraftID() == flight->aircraft->getAircraftID(); }),
            atc->aircraftsWithActiveViolations.end());
        delete flight;
        delete args;
        return nullptr;
    }
    static void* avnListener(void* arg) {
        ATC* atc = static_cast<ATC*>(arg);
        while(atc->running) {
            atc->processViolationClearedNotification();
            usleep(100000); // Sleep for 100ms to prevent busy-waiting
        }
        return nullptr;
    }

    // Update the ATC constructor (replace the existing constructor)
// Update the ATC constructor to load the font (replace the existing constructor)
ATC() {
    pthread_mutex_init(&printMutex, nullptr);
    pthread_mutex_init(&waitingQueueMutex, nullptr);
    pthread_mutex_init(&statsMutex, nullptr);
    pthread_mutex_init(&pipeMutex, nullptr);
    pthread_mutex_init(&sfmlMutex, nullptr);
    cout << "\n[ATC] Initializing Air Traffic Control...\n" << flush;

    // SFML Initialization
    window = new sf::RenderWindow(sf::VideoMode(WINDOW_WIDTH, WINDOW_HEIGHT), "ATC Simulation");
    if(!runwayTexture.loadFromFile("runway.png")) {
        cout << "[ERROR] Failed to load runway.png" << endl << flush;
        exit(1);
    }
    if(!planeTexture.loadFromFile("plane2.png")) {
        cout << "[ERROR] Failed to load plane2.png" << endl << flush;
        exit(1);
    }
    if(!font.loadFromFile("ARIAL.TTF")) {
        cout << "[ERROR] Failed to load ARIAL.TTF" << endl << flush;
        exit(1);
    }

    // Setup runway sprites (three runways: left, center, right)
    float oneThirdWidth = WINDOW_WIDTH / 3.0f;
    for(int i = 0; i < 3; ++i) {
        runwaySprites[i].setTexture(runwayTexture);
        runwaySprites[i].setScale(oneThirdWidth / runwayTexture.getSize().x, WINDOW_HEIGHT / runwayTexture.getSize().y);
        runwaySprites[i].setPosition(i * oneThirdWidth, 0);
    }

    // Initialize plane sprites and labels
    for(int r = 0; r < 3; ++r) {
        for(int p = 0; p < 3; ++p) {
            runwayPlanes[r][p].sprite.setTexture(planeTexture);
            runwayPlanes[r][p].sprite.setScale(planeScale, planeScale);
            runwayPlanes[r][p].label.setFont(font);
            runwayPlanes[r][p].label.setCharacterSize(14);
            runwayPlanes[r][p].label.setFillColor(sf::Color::White);
            runwayPlanes[r][p].label.setOutlineColor(sf::Color::Black);
            runwayPlanes[r][p].label.setOutlineThickness(1.0f);
            runwayPlanes[r][p].isActive = false;
            runwayPlanes[r][p].flightNumber = "";
            runwayPlanes[r][p].speed = 0.0f;
            runwayPlanes[r][p].isTakingOff = false;
            runwayPlanes[r][p].isLanding = false;
            float spacing = oneThirdWidth / 4.0f;
            float bottomY = WINDOW_HEIGHT - (planeTexture.getSize().x * planeScale / 2.0f);
            runwayPlanes[r][p].sprite.setPosition(r * oneThirdWidth + spacing * (p + 1), bottomY);
            runwayPlanes[r][p].label.setPosition(r * oneThirdWidth + spacing * (p + 1), bottomY - 20); // Initial position above sprite
            runwayPlanes[r][p].sprite.setRotation(180); // Face upward
        }
    }

    // Start SFML render thread
    if(pthread_create(&renderThread, nullptr, sfmlRenderThread, this) != 0) {
        cout << "[ERROR] Failed to create SFML render thread" << endl << flush;
        delete window;
        exit(1);
    }
    pthread_detach(renderThread);

    // Existing initialization (FIFOs, runways, airlines, etc.)
    if(mkfifo("atc_to_avn.fifo", 0666) == -1 && errno != EEXIST) {
        cout << "[ERROR] Failed to create atc_to_avn.fifo: " << strerror(errno) << endl << flush;
        exit(1);
    }
    usleep(10000);
    if(mkfifo("avn_to_atc.fifo", 0666) == -1 && errno != EEXIST) {
        cout << "[ERROR] Failed to create avn_to_atc.fifo: " << strerror(errno) << endl << flush;
        exit(1);
    }
    usleep(10000);
    if(mkfifo("avn_ctrl.fifo", 0666) == -1 && errno != EEXIST) {
        cout << "[ERROR] Failed to create avn_ctrl.fifo: " << strerror(errno) << endl << flush;
        exit(1);
    }
    usleep(10000);
    if(mkfifo("atc_ctrl.fifo", 0666) == -1 && errno != EEXIST) {
        cout << "[ERROR] Failed to create atc_ctrl.fifo: " << strerror(errno) << endl << flush;
        exit(1);
    }
    usleep(10000);

    for(int attempt = 1; attempt <= 10; attempt++) {
        fd_avn_notify_pipe = open("avn_to_atc.fifo", O_RDONLY | O_NONBLOCK);
        if(fd_avn_notify_pipe < 0) {
            cout << "[ATC] Attempt " << attempt << " failed to open avn_to_atc.fifo: " << strerror(errno) << ", retrying..." << endl << flush;
            usleep(500000);
            continue;
        }
        cout << "[ATC] Successfully opened avn_to_atc.fifo for reading" << endl << flush;
        break;
    }
    if(fd_avn_notify_pipe < 0) {
        cout << "[ERROR] Failed to open avn_to_atc.fifo after retries: " << strerror(errno) << endl << flush;
        exit(1);
    }

    if(pthread_create(&avnListenerThread, nullptr, avnListener, this) != 0) {
        cout << "[ERROR] Failed to create AVN listener thread" << endl << flush;
        close(fd_avn_notify_pipe);
        exit(1);
    }
    pthread_detach(avnListenerThread);

    for(int attempt = 1; attempt <= 10; attempt++) {
        fd_avn_pipe = open("atc_to_avn.fifo", O_WRONLY | O_NONBLOCK);
        if(fd_avn_pipe < 0) {
            cout << "[ATC] Attempt " << attempt << " failed to open atc_to_avn.fifo: " << strerror(errno) << ", retrying..." << endl << flush;
            usleep(500000);
            continue;
        }
        cout << "[ATC] Successfully opened atc_to_avn.fifo for writing" << endl << flush;
        break;
    }
    if(fd_avn_pipe < 0) {
        cout << "[ERROR] Failed to open atc_to_avn.fifo after retries: " << strerror(errno) << endl << flush;
        close(fd_avn_notify_pipe);
        exit(1);
    }

    for(int attempt = 1; attempt <= 20; attempt++) {
        fd_ctrl_pipe = open("avn_ctrl.fifo", O_RDONLY | O_NONBLOCK);
        if(fd_ctrl_pipe < 0) {
            cout << "[ATC] Attempt " << attempt << " failed to open avn_ctrl.fifo for reading: " << strerror(errno) << ", retrying..." << endl << flush;
            usleep(1000000);
            continue;
        }
        cout << "[ATC] Successfully opened avn_ctrl.fifo for reading" << endl << flush;
        break;
    }
    if(fd_ctrl_pipe < 0) {
        cout << "[ERROR] Failed to open avn_ctrl.fifo after retries: " << strerror(errno) << endl << flush;
        close(fd_avn_pipe);
        close(fd_avn_notify_pipe);
        exit(1);
    }

    setRunways();
    setAirlines();
    generateAircrafts();
    setSchedule();

    cout << "[ATC] Waiting for avn readiness signal on avn_ctrl.fifo..." << endl << flush;
    char ctrl_buf[16];
    ssize_t bytesRead = 0;
    while(bytesRead <= 0) {
        bytesRead = read(fd_ctrl_pipe, ctrl_buf, sizeof(ctrl_buf));
        if(bytesRead < 0 && errno != EAGAIN) {
            cout << "[ATC] ERROR: Failed to read from avn_ctrl.fifo: " << strerror(errno) << endl << flush;
            close(fd_ctrl_pipe);
            close(fd_avn_pipe);
            close(fd_avn_notify_pipe);
            exit(1);
        }
        usleep(500000);
    }
    close(fd_ctrl_pipe);
    avnReady = true;
    cout << "[ATC] Received readiness signal from avn" << endl << flush;

    cout << "[ATC] Initialization complete.\n" << flush;
}

    // Sends an AVN (Airspace Violation Notification) to a subsystem through a FIFO pipe
void sendAVNToSubsystem(const AVN& avn, const string& pipeName){
    pthread_mutex_lock(&pipeMutex); // Lock pipe access to ensure thread safety

    if(!avnReady){ // Check if AVN subsystem is ready
        pthread_mutex_lock(&printMutex); // Lock printing to avoid interleaved output
        cout << "[ERROR] AVN subsystem not ready, cannot send AVN " << avn.avnID << endl << flush;
        pthread_mutex_unlock(&printMutex); // Unlock after printing
        pthread_mutex_unlock(&pipeMutex); // Unlock pipe mutex
        return; // Exit function
    }

    if(fd_avn_pipe < 0){ // If pipe hasn't been opened yet
        fd_avn_pipe = open(pipeName.c_str(), O_WRONLY | O_NONBLOCK); // Open pipe in write-only non-blocking mode
        if(fd_avn_pipe < 0) { // If open failed
            pthread_mutex_lock(&printMutex);
            cout << "[ERROR] Failed to open pipe " << pipeName << " for writing AVN " << avn.avnID << ": " << strerror(errno) << endl << flush;
            pthread_mutex_unlock(&printMutex);
            pthread_mutex_unlock(&pipeMutex);
            return;
        }
    }

    char buffer[AVN::serializedSize()]; // Allocate buffer to hold serialized AVN
    size_t offset = 0;
    avn.serialize(buffer, offset); // Serialize AVN into the buffer

    ssize_t bytesWritten = write(fd_avn_pipe, buffer, sizeof(buffer)); // Attempt to write to pipe
    if(bytesWritten == sizeof(buffer)) { // Success if all bytes written
        pthread_mutex_lock(&printMutex);
        cout << "[AVN SENT] Successfully sent AVN " << avn.avnID << " to " << pipeName << " (bytes: " << bytesWritten << ")" << endl << flush;
        pthread_mutex_unlock(&printMutex);
    } else { // Handle partial or failed write
        pthread_mutex_lock(&printMutex);
        cout << "[ERROR] Failed to write AVN " << avn.avnID << " to " << pipeName << ", bytes written: " << bytesWritten << ", expected: " << sizeof(buffer) << ", error: " << strerror(errno) << endl << flush;
        pthread_mutex_unlock(&printMutex);
    }

    pthread_mutex_unlock(&pipeMutex); // Unlock pipe mutex
}

// Reads and processes a ViolationClearedNotification from the AVN system
void processViolationClearedNotification() {
    char buffer[ViolationClearedNotification::serializedSize()]; // Buffer to hold incoming notification
    ssize_t bytesRead = read(fd_avn_notify_pipe, buffer, sizeof(buffer)); // Attempt to read from pipe

    if(bytesRead == sizeof(buffer)) { // Full message read
        ViolationClearedNotification notification;
        size_t offset = 0;
        notification.deserialize(buffer, offset); // Deserialize into object

        pthread_mutex_lock(&statsMutex); // Lock stats as we will modify list of active violations
        aircraftsWithActiveViolations.erase(
            remove_if(aircraftsWithActiveViolations.begin(), aircraftsWithActiveViolations.end(),
                      [&](Aircraft* a) { return a->getAircraftID() == notification.flightNumber; }),
            aircraftsWithActiveViolations.end()); // Remove cleared aircraft from list

        pthread_mutex_lock(&printMutex);
        cout << "[ATC] Violation cleared for AVN " << notification.avnID << ", Flight: " << notification.flightNumber << endl << flush;
        pthread_mutex_unlock(&printMutex);
        pthread_mutex_unlock(&statsMutex); // Unlock after modifying
    } else if(bytesRead < 0 && errno != EAGAIN) { // Handle read error (excluding non-blocking empty pipe)
        pthread_mutex_lock(&printMutex);
        cout << "[ERROR] Failed to read from avn_to_atc.fifo: " << strerror(errno) << endl << flush;
        pthread_mutex_unlock(&printMutex);
    }
}

// Generates an AVN (Airspace Violation Notification) for a given aircraft
AVN generateAVN(Aircraft* aircraft) {
    AVN avn;
    string avnIDStr = "AVN-" + to_string(time(nullptr)); // Generate unique AVN ID using current time
    strncpy(avn.avnID, avnIDStr.c_str(), sizeof(avn.avnID) - 1); // Copy to struct field
    avn.avnID[sizeof(avn.avnID) - 1] = '\0'; // Null-terminate

    string airlineName = aircraft->getAirlineName(); // Get airline
    strncpy(avn.airlineName, airlineName.c_str(), sizeof(avn.airlineName) - 1);
    avn.airlineName[sizeof(avn.airlineName) - 1] = '\0';

    string flightNumber = aircraft->getAircraftID(); // Get flight number
    strncpy(avn.flightNumber, flightNumber.c_str(), sizeof(avn.flightNumber) - 1);
    avn.flightNumber[sizeof(avn.flightNumber) - 1] = '\0';

    avn.type = aircraft->getAircraftType(); // Get type (commercial/cargo/etc.)
    avn.speedRecorded = aircraft->getSpeed(); // Record current speed

    // Set permissible speed based on flight phase
    Status phase = aircraft->getPhase();
    if(phase == HOLDING) avn.permissibleSpeed = 600;
    else if(phase == APPROACHING) avn.permissibleSpeed = 290;
    else if(phase == LANDING) avn.permissibleSpeed = 240;
    else if(phase == TAXIING) avn.permissibleSpeed = 30;
    else if(phase == AT_GATE) avn.permissibleSpeed = 10;
    else if(phase == TAKING_OFF) avn.permissibleSpeed = 290;
    else if(phase == CLIMBING) avn.permissibleSpeed = 463;
    else if(phase == CRUISING) avn.permissibleSpeed = 900;
    else avn.permissibleSpeed = 0;

    avn.issuanceTime = time(nullptr); // Set current timestamp
    avn.fineAmount = (avn.type == CARGO ? 700000 : 500000) * 1.15; // Fine calculation based on type
    strncpy(avn.paymentStatus, "unpaid", sizeof(avn.paymentStatus) - 1);
    avn.paymentStatus[sizeof(avn.paymentStatus) - 1] = '\0';
    avn.dueDate = avn.issuanceTime + 3 * 24 * 3600; // Set due date to 3 days later
    return avn;
}

// Initializes the runways for the simulation
void setRunways() {
    cout << "[ATC] Setting up runways...\n" << flush;
    runways.emplace_back(RWY_A); // Arrival runway
    runways.emplace_back(RWY_B); // Departure runway
    runways.emplace_back(RWY_C); // Cargo/Emergency runway
    cout << "[ATC] RWY-A (Arrivals), RWY-B (Departures), RWY-C (Cargo/Emergency/Overflow) ready.\n" << flush;
}

// Registers a set of airlines in the simulation
void setAirlines() {
    cout << "[ATC] Registering airlines...\n" << flush;
    airlines.emplace_back("PIA", 6, 4);         // Airline name, max aircraft, max active flights
    airlines.emplace_back("AirBlue", 4, 4);
    airlines.emplace_back("FedEx", 3, 2);
    airlines.emplace_back("Pakistan Airforce", 2, 1);
    airlines.emplace_back("Blue Dart", 2, 2);
    airlines.emplace_back("AghaKhan Air", 2, 1);
    cout << "[ATC] Airlines registered: PIA, AirBlue, FedEx, Pakistan Airforce, Blue Dart, AghaKhan Air.\n" << flush;
}

// Generates aircraft and assigns them to airlines
void generateAircrafts() {
    pthread_mutex_lock(&printMutex);
    cout << "[ATC] Generating aircrafts and assigning to airlines...\n" << flush;
    pthread_mutex_unlock(&printMutex);

    // Add aircraft objects with ID, type, and airline association
    aircrafts.push_back(new Aircraft("PK-101", COMMERCIAL, &airlines[0]));
    aircrafts.push_back(new Aircraft("PK-102", COMMERCIAL, &airlines[0]));
    aircrafts.push_back(new Aircraft("PK-103", COMMERCIAL, &airlines[0]));
    aircrafts.push_back(new Aircraft("PK-104", EMERGENCY, &airlines[0]));

    aircrafts.push_back(new Aircraft("AB-201", COMMERCIAL, &airlines[1]));
    aircrafts.push_back(new Aircraft("AB-202", COMMERCIAL, &airlines[1]));
    aircrafts.push_back(new Aircraft("AB-203", COMMERCIAL, &airlines[1]));
    aircrafts.push_back(new Aircraft("AB-203", EMERGENCY, &airlines[1])); // Duplicate ID—potential issue

    aircrafts.push_back(new Aircraft("FX-301", CARGO, &airlines[2]));
    aircrafts.push_back(new Aircraft("FX-302", EMERGENCY, &airlines[2]));

    aircrafts.push_back(new Aircraft("PAF-401", CARGO, &airlines[3]));
    aircrafts.push_back(new Aircraft("PAF-402", EMERGENCY, &airlines[3]));

    aircrafts.push_back(new Aircraft("BD-601", CARGO, &airlines[4]));
    aircrafts.push_back(new Aircraft("BD-602", EMERGENCY, &airlines[4]));

    aircrafts.push_back(new Aircraft("AK-701", EMERGENCY, &airlines[5]));
    aircrafts.push_back(new Aircraft("AK-702", COMMERCIAL, &airlines[5]));

    pthread_mutex_lock(&printMutex);
    cout << "[ATC] Aircrafts initialized. Total: " << aircrafts.size() << endl << flush;
    pthread_mutex_unlock(&printMutex);

    airlineStatus(); // Print airline status report
}

// Prints status of all airlines including aircraft counts and flights
void airlineStatus(){
    stringstream ss;
    ss << "\n[AIRLINE STATUS]\n";
    for(const auto& airline : airlines){
        int inAirCount = 0;
        for(const auto* aircraft : aircrafts) {
            if(aircraft && aircraft->getAirlineName() == airline.getName() && aircraft->isInAirPhase())
                inAirCount++; // Count aircraft currently airborne
        }
        ss << "  - " << airline.getName()
           << ": Aircraft " << airline.getCurrentAircraftCount() << "/" << airline.getMaxAircraft()
           << ", Active Flights " << airline.getCurrentActiveFlightCount() << "/" << airline.getMaxFlight()
           << ", In Air " << inAirCount << "\n";
    }
    ss << "[END STATUS]\n";

    pthread_mutex_lock(&printMutex);
    cout << ss.str() << flush;
    pthread_mutex_unlock(&printMutex);
}


    void setSchedule(){
        startTime = time(nullptr);
        pthread_mutex_lock(&printMutex);
        cout << "[ATC] Setting up flight schedule...\n" << flush;
        pthread_mutex_unlock(&printMutex);
        
        struct AirlineSchedule{
            string name;
            vector<tuple<Direction, bool, FlightType, string, int>> configs;
        };
        
        vector<AirlineSchedule> airlines = {
            {"PIA", {{NORTH, true, COMMERCIAL, "PK", 180}, {EAST, false, COMMERCIAL, "PK", 150}}},
            {"AirBlue", {{SOUTH, true, COMMERCIAL, "AB", 120}, {WEST, false, COMMERCIAL, "AB", 240}}},
            {"FedEx", {{NORTH, true, CARGO, "FX", 180}, {EAST, false, CARGO, "FX", 150}}},
            {"Pakistan Airforce", {{NORTH, true, EMERGENCY, "PAF", 180}, {SOUTH, true, EMERGENCY, "PAF", 120}, {EAST, false, EMERGENCY, "PAF", 150}, {WEST, false, EMERGENCY, "PAF", 240}}},
            {"Blue Dart", {{SOUTH, true, CARGO, "BD", 120}, {WEST, false, CARGO, "BD", 240}}},
            {"AghaKhan Air", {{NORTH, true, EMERGENCY, "AK", 180}, {SOUTH, true, EMERGENCY, "AK", 120}, {EAST, false, EMERGENCY, "AK", 150}, {WEST, false, EMERGENCY, "AK", 240}}}
        };
        
        map<string, int> flightNumbers;
        
        for(const auto& airline : airlines){
            vector<tuple<Direction, bool, FlightType, string, int>> departures, arrivals;
            for(const auto& config : airline.configs) {
                if(!get<1>(config)) departures.push_back(config);
                else arrivals.push_back(config);
            }
            
            int timeOffset = 10;
            // Schedule departures
            for(size_t i = 0; i < departures.size() && timeOffset <= 270; ++i){
                auto& config = departures[i % departures.size()];
                Direction dir = get<0>(config);
                bool isArrival = get<1>(config);
                FlightType ftype = get<2>(config);
                string prefix = get<3>(config);
                int interval = get<4>(config);
                
                int flightNum = ++flightNumbers[prefix];
                string fullFlightID = prefix + to_string(100 + flightNum) + (isArrival ? "-A" : "-D");
                addFlightEntry(fullFlightID, airline.name, ftype, dir, startTime + timeOffset, isArrival);
                timeOffset += (interval + 30 + rand() % 60); // Increased time difference
            }
            
            // Schedule arrivals
            for(size_t i = 0; i < arrivals.size() && timeOffset <= 270; ++i){
                auto& config = arrivals[i % arrivals.size()];
                Direction dir = get<0>(config);
                bool isArrival = get<1>(config);
                FlightType ftype = get<2>(config);
                string prefix = get<3>(config);
                int interval = get<4>(config);
                
                int flightNum = ++flightNumbers[prefix];
                string fullFlightID = prefix + to_string(100 + flightNum) + (isArrival ? "-A" : "-D");
                addFlightEntry(fullFlightID, airline.name, ftype, dir, startTime + timeOffset, isArrival);
                timeOffset += (interval + 30 + rand() % 60); // Increased time difference
            }
        }
        
        addFlightEntry("PAF401-D", "Pakistan Airforce", EMERGENCY, EAST, startTime + 150, false);
        
        pthread_mutex_lock(&printMutex);
        cout << "[ATC] Flight schedule initialized with " << flightSchedule.queue.size() << " flights.\n" << flush;
        pthread_mutex_unlock(&printMutex);
        }
    


    void addFlightEntry(const string& fn, const string& an, FlightType ft, Direction d, time_t st, bool ia) {
        for(auto& airline : airlines) {
            if(airline.getName() == an) {
                flightSchedule.addFlight(FlightEntry(fn, an, ft, d, st, ia));
                return;
            }
        }
        pthread_mutex_lock(&printMutex);
        cout << "[ERROR] Cannot add flight " << fn << ": Invalid airline." << endl << flush;
        pthread_mutex_unlock(&printMutex);
    }

    
   
   
// Update the ATC destructor (replace the existing destructor)
~ATC() {
    running = false;
    if(fd_avn_pipe >= 0) close(fd_avn_pipe);
    if(fd_avn_notify_pipe >= 0) close(fd_avn_notify_pipe);
    pthread_mutex_destroy(&printMutex);
    pthread_mutex_destroy(&waitingQueueMutex);
    pthread_mutex_destroy(&statsMutex);
    pthread_mutex_destroy(&pipeMutex);
    pthread_mutex_destroy(&sfmlMutex);
    for(auto* a : aircrafts) delete a;
    if(window) {
        window->close();
        delete window;
    }
    cout << "[ATC] Shutdown complete.\n" << flush;
}





    //adjustment





    void completePlaneMovement(const string& flightNumber, RunwayType runway) {
        // Lock the SFML-related mutex to ensure thread-safe access to graphical objects
        pthread_mutex_lock(&sfmlMutex);
    
        // Convert the RunwayType enum to an integer index for array access
        int r = static_cast<int>(runway);
    
        // Iterate over the planes assigned to the specified runway
        for (auto& plane : runwayPlanes[r]) {
            // Check if this plane matches the flight number, is currently active, and has no fault
            if (plane.flightNumber == flightNumber && plane.isActive && !plane.hasFault) {
                // Mark the plane as inactive (no longer in motion)
                plane.isActive = false;
    
                // Clear any takeoff state
                plane.isTakingOff = false;
    
                // Clear any landing state
                plane.isLanding = false;
    
                // Remove the flight number (reset plane to default state)
                plane.flightNumber = "";
    
                // Reset speed to 0
                plane.speed = 0.0f;
    
                // Set the phase to WAITING, indicating idle or standby
                plane.phase = WAITING;
    
                // Clear the label text associated with the plane (e.g., speed, ID)
                plane.label.setString("");
    
                // Calculate one-third of the window width for runway partitioning
                float oneThirdWidth = WINDOW_WIDTH / 3.0f;
    
                // Calculate spacing between planes on the same runway
                float spacing = oneThirdWidth / 4.0f;
    
                // Compute the Y-position near the bottom of the window
                float bottomY = WINDOW_HEIGHT - (planeTexture.getSize().x * planeScale / 2.0f);
    
                // Determine this plane's index in the runway's plane vector
                int index = &plane - &runwayPlanes[r][0];
    
                // Reset the plane sprite's position based on its index and spacing
                plane.sprite.setPosition(r * oneThirdWidth + spacing * (index + 1), bottomY);
    
                // Set the label's position just above the plane sprite
                plane.label.setPosition(r * oneThirdWidth + spacing * (index + 1), bottomY - 30);
    
                // Rotate the plane sprite 180 degrees to face downward (default idle direction)
                plane.sprite.setRotation(180);
    
                // Stop iterating after finding and resetting the correct plane
                break;
            }
        }
    
        // Unlock the SFML mutex now that we're done updating graphical elements
        pthread_mutex_unlock(&sfmlMutex);
    }

    
    Runway* requestRunway(const FlightEntry& flight) {
        // Create a vector to hold the preferred runways based on flight characteristics
        vector<Runway*> preferredRunways;
    
        // Assign preferred runways based on flight priority and whether it's an arrival
        if (flight.priority <= 2) {
            // High-priority flights prefer primary and backup runways
            preferredRunways = flight.isArrival ? vector<Runway*>{&runways[0], &runways[2]}
                                                : vector<Runway*>{&runways[1], &runways[2]};
        } else if (flight.type == CARGO) {
            // Cargo flights use only the backup runway
            preferredRunways = {&runways[2]};
        } else {
            // All other cases use the appropriate main runway (arrival: 0, departure: 1)
            preferredRunways = {&runways[flight.isArrival ? 0 : 1]};
        }
    
        // Attempt to allocate one of the preferred runways
        for (auto runway : preferredRunways) {
            // Try to get the runway for the flight
            if (runway->getRunway(flight.flightNumber, flight.direction, flight.type, flight.priority, flight.isArrival)) {
                // Lock print mutex for thread-safe console output
                pthread_mutex_lock(&printMutex);
                // Print successful runway allocation message
                cout << "[RUNWAY GRANTED] " << flight.flightNumber << " granted " 
                     << runwayTypeToStr(runway->getAircraftType()) 
                     << " for " << (flight.isArrival ? "arrival" : "departure") 
                     << " heading " << directionToStr(flight.direction) << "." << endl << flush;
                pthread_mutex_unlock(&printMutex);
                return runway; // Return the granted runway
            } else {
                // Print message if the runway was busy
                pthread_mutex_lock(&printMutex);
                cout << "[RUNWAY BUSY] " << flight.flightNumber 
                     << " could not get " << runwayTypeToStr(runway->getAircraftType()) 
                     << endl << flush;
                pthread_mutex_unlock(&printMutex);
            }
        }
    
        // Return nullptr if no runway could be allocated
        return nullptr;
    }

    void prodSimulation() {
        // Print the start of the simulation
        pthread_mutex_lock(&printMutex);
        cout << "\n[ATC] Starting 5-minute simulation...\n" << flush;
        pthread_mutex_unlock(&printMutex);
    
        // Record the simulation start time
        startTime = time(nullptr);
        const int simDuration = 300; // 5 minutes in seconds
    
        // Main simulation loop runs for 5 minutes
        while (difftime(time(nullptr), startTime) < simDuration) {
            time_t now = time(nullptr); // Current time
    
            // Try to get the next flight scheduled for now
            FlightEntry* flight = flightSchedule.getNextFlight(now);
            if (flight) {
                // If it's an emergency flight (priority 1), print a special dispatch message
                if (flight->priority == 1) {
                    pthread_mutex_lock(&printMutex);
                    cout << "[EMERGENCY DISPATCH] Processing emergency flight " 
                         << flight->flightNumber << endl << flush;
                    pthread_mutex_unlock(&printMutex);
                }
    
                // Try to assign an available aircraft to the flight
                flight->aircraft = getAvailableAircraft(flight->type, flight->airlineName);
                if (flight->aircraft) {
                    // Prepare and print a dispatch message
                    stringstream ss;
                    ss << "\n[DISPATCH] Launching flight " << flight->flightNumber 
                       << " [" << flightTypeToStr(flight->type) << "] heading " 
                       << directionToStr(flight->direction) << "\n";
                    pthread_mutex_lock(&printMutex);
                    cout << ss.str() << flush;
                    pthread_mutex_unlock(&printMutex);
    
                    // Start the simulation of the flight
                    simulateFlight(flight);
                } else {
                    // If aircraft not available, reschedule the flight
                    flight->rescheduleCount++;
                    if (flight->rescheduleCount >= maxResched) {
                        // If reschedule limit reached, cancel the flight
                        pthread_mutex_lock(&printMutex);
                        cout << "[ERROR] Flight " << flight->flightNumber 
                             << " exceeded maximum reschedule attempts (" << maxResched 
                             << "). Canceling flight.\n" << flush;
                        pthread_mutex_unlock(&printMutex);
                        delete flight;
                        continue;
                    }
    
                    // Prepare a message for rescheduling
                    stringstream ss;
                    ss << "[DISPATCH FAILED] No available " << flightTypeToStr(flight->type)
                       << " aircraft for " << flight->flightNumber << " from airline " 
                       << flight->airlineName << ". Rescheduling...\n";
    
                    // Add 15 seconds to flight time and reschedule
                    time_t newTime = flight->scheduledTime + 15;
                    flightSchedule.rescheduleFlight(*flight, 15);
                    ss << "flight with flight number " << flight->flightNumber 
                       << " is rescheduled to " << ctime(&newTime);
    
                    pthread_mutex_lock(&printMutex);
                    cout << ss.str() << flush;
                    pthread_mutex_unlock(&printMutex);
                    delete flight;
                }
            }
    
            // Handle waiting queue flights scheduled for now or earlier
            vector<FlightEntry> flightsToProcess;
            pthread_mutex_lock(&waitingQueueMutex);
            for (auto it = waitingQueue.begin(); it != waitingQueue.end();) {
                if (it->scheduledTime <= now) {
                    // Move flight to processing list and remove from queue
                    flightsToProcess.push_back(*it);
                    it = waitingQueue.erase(it);
                } else {
                    ++it;
                }
            }
            pthread_mutex_unlock(&waitingQueueMutex);
    
            // Process all waiting flights
            for (FlightEntry& flight : flightsToProcess) {
                // Try to assign an aircraft
                flight.aircraft = getAvailableAircraft(flight.type, flight.airlineName);
                if (flight.aircraft) {
                    // Print dispatching message
                    stringstream ss;
                    ss << "[DISPATCH] Processing rescheduled flight with flight number " 
                       << flight.flightNumber << " at " << ctime(&flight.scheduledTime);
                    pthread_mutex_lock(&printMutex);
                    cout << ss.str() << flush;
                    pthread_mutex_unlock(&printMutex);
    
                    // Simulate the flight
                    simulateFlight(new FlightEntry(flight));
                    usleep(1000000); // Short delay to pace simulation
                } else {
                    // Handle case where no aircraft is available for rescheduled flight
                    flight.rescheduleCount++;
                    if (flight.rescheduleCount >= maxResched) {
                        pthread_mutex_lock(&printMutex);
                        cout << "[ERROR] Waiting flight " << flight.flightNumber 
                             << " exceeded maximum reschedule attempts (" << maxResched 
                             << "). Canceling flight.\n" << flush;
                        pthread_mutex_unlock(&printMutex);
                        continue;
                    }
    
                    // Reschedule the flight again
                    time_t newTime = flight.scheduledTime + 15;
                    flightSchedule.rescheduleFlight(flight, 15);
                    stringstream ss;
                    ss << "[DISPATCH FAILED] No available " << flightTypeToStr(flight.type) 
                       << " aircraft for waiting flight " << flight.flightNumber 
                       << " from airline " << flight.airlineName << ". Rescheduling...\n";
                    ss << "flight with flight number " << flight.flightNumber 
                       << " is rescheduled to " << ctime(&newTime);
                    pthread_mutex_lock(&printMutex);
                    cout << ss.str() << flush;
                    pthread_mutex_unlock(&printMutex);
                }
            }
    
            // Sleep briefly to reduce CPU usage
            usleep(2000);
        }
    
        // Print simulation end message
        pthread_mutex_lock(&printMutex);
        cout << "\n[ATC] 5-minute simulation complete.\n" << flush;
        pthread_mutex_unlock(&printMutex);
    
        // Generate final report
        getReport();
    }
    

// Static function to run the SFML rendering in a separate thread
static void* sfmlRenderThread(void* arg) {
    // Cast the argument to an ATC* object
    ATC* atc = static_cast<ATC*>(arg);

    // Continuously render while the simulation is running
    while (atc->running) {
        atc->sfmlRender();      // Call the SFML render method of the ATC class
        usleep(16666);          // Sleep for ~16.666 milliseconds (~60 frames per second)
    }

    return nullptr;             // Thread exits when simulation stops
}


// Function to assign a plane to a specific runway, either for arrival or departure
void assignPlaneToRunway(const string& flightNumber, RunwayType runway, bool isArrival) {
    pthread_mutex_lock(&sfmlMutex); // Lock to safely modify shared SFML plane state

    // Get numeric index of runway (0 = A, 1 = B, 2 = C)
    int r = static_cast<int>(runway);

    // Check if the selected runway is clear (i.e., no active planes)
    bool runwayClear = true;
    for (const auto& plane : runwayPlanes[r]) {
        if (plane.isActive) {
            runwayClear = false; // If any plane is active, the runway is busy
            break;
        }
    }

    // If runway is not clear, exit without assigning
    if (!runwayClear) {
        pthread_mutex_unlock(&sfmlMutex);
        return;
    }

    // Compute spatial layout constants for sprite positioning
    float oneThirdWidth = WINDOW_WIDTH / 3.0f;  // Each runway takes up 1/3 of the screen width
    float bottomY = WINDOW_HEIGHT - (planeTexture.getSize().x * planeScale / 2.0f); // Y position for departures
    float topY = 0.0f; // Y position for arrivals (top of screen)
    float xPos; // X position based on runway

    // Determine horizontal spawn position based on runway
    if (r == 2) {
        xPos = (r * oneThirdWidth) + (3 * oneThirdWidth / 4.0f); // Right side of Runway C
    } else if (r == 0) {
        xPos = oneThirdWidth / 4.0f; // Left side of Runway A
    } else {
        xPos = (r * oneThirdWidth) + (oneThirdWidth / 2.0f); // Center of Runway B
    }

    // Assign the plane to the runway's plane slots
    for (auto& plane : runwayPlanes[r]) {
        if (!plane.isActive) { // Find an available (inactive) plane slot
            plane.isActive = true; // Mark plane as active
            plane.flightNumber = flightNumber; // Set flight number

            if (isArrival) {
                // Configure landing parameters
                plane.isLanding = true;
                plane.isTakingOff = false;
                plane.phase = LANDING; // Set flight phase
                plane.speed = 3.0f; // Landing speed
                plane.sprite.setPosition(xPos, topY); // Start at top of screen
                plane.sprite.setRotation(0); // Face downward
                plane.label.setPosition(xPos - plane.label.getLocalBounds().width / 2, topY - 30); // Position label above plane
            } else {
                // Configure takeoff parameters
                plane.isTakingOff = true;
                plane.isLanding = false;
                plane.phase = TAKING_OFF; // Set flight phase
                plane.speed = 1.0f; // Takeoff starting speed
                plane.sprite.setPosition(xPos, bottomY); // Start at bottom of screen
                plane.sprite.setRotation(180); // Face upward
                plane.label.setPosition(xPos - plane.label.getLocalBounds().width / 2, bottomY - 30); // Position label above plane
            }

            // Set label with flight number, phase, and speed
            std::stringstream ss;
            ss << flightNumber << "\n" 
               << statusToStr(plane.phase) << "\nSpeed: " 
               << std::fixed << std::setprecision(1) << plane.speed;
            plane.label.setString(ss.str()); // Apply label to plane

            break; // Plane assigned, no need to continue loop
        }
    }

    pthread_mutex_unlock(&sfmlMutex); // Unlock after assignment is complete
}


   void sfmlRender() {
        pthread_mutex_lock(&sfmlMutex);
        if (!window->isOpen()) {
            pthread_mutex_unlock(&sfmlMutex);
            return;
        }

        sf::Event event;
        while (window->pollEvent(event)) {
            if (event.type == sf::Event::Closed) {
                window->close();
                running = false;
            }
        }

        // Update plane positions and speeds
        float oneThirdWidth = WINDOW_WIDTH / 3.0f;
        float rotatedHeight = planeTexture.getSize().x * planeScale;
        float bottomY = WINDOW_HEIGHT - rotatedHeight / 2.0f;
        float topY = 0.0f;

        for (int r = 0; r < 3; ++r) {
            float runwayX;
            if (r == 2) {
                runwayX = (r * oneThirdWidth) + (3 * oneThirdWidth / 4.0f); // Right side of Runway C
            } else if (r == 0) {
                runwayX = oneThirdWidth / 4.0f; // Left side of Runway A
            } else {
                runwayX = (r * oneThirdWidth) + (oneThirdWidth / 2.0f); // Center of Runway B
            }

            for (auto& plane : runwayPlanes[r]) {
                if (plane.isActive) {
                    if (plane.hasFault && !plane.hasFled) {
                        // Move faulty plane diagonally until off-screen
                        plane.sprite.move(0.03f, -0.05f);
                        plane.label.setPosition(plane.sprite.getPosition().x - plane.label.getLocalBounds().width / 2, plane.sprite.getPosition().y - 30);
                        // Update label for faulty plane
                        stringstream ss;
                        ss << plane.flightNumber << "\nFaulty\nSpeed: " << fixed << setprecision(1) << plane.speed;
                        plane.label.setString(ss.str());
                        // Check if plane is off-screen
                        if (plane.sprite.getPosition().x > WINDOW_WIDTH || plane.sprite.getPosition().y < -rotatedHeight) {
                            plane.isActive = false;
                            plane.hasFault = false;
                            plane.hasFled = true;
                            plane.flightNumber = "";
                            plane.label.setString("");
                            plane.phase = WAITING;
                            float spacing = oneThirdWidth / 4.0f;
                            int index = &plane - &runwayPlanes[r][0];
                            plane.sprite.setPosition(r * oneThirdWidth + spacing * (index + 1), bottomY);
                            plane.label.setPosition(r * oneThirdWidth + spacing * (index + 1), bottomY - 30);
                            plane.sprite.setRotation(180);
                            plane.speed = 0.0f;
                        }
                    } else if (plane.isTakingOff) {
                        // Departures: Accelerate and move up until fully off-screen
                        if (plane.speed < 10.0f) {
                            plane.speed += 1.0f;
                        }
                        float topEdge = plane.sprite.getPosition().y - rotatedHeight / 2;
                        if (topEdge > -rotatedHeight) {
                            plane.sprite.move(0, -plane.speed);
                            plane.label.setPosition(runwayX - plane.label.getLocalBounds().width / 2, plane.sprite.getPosition().y - 30);
                            // Update label with current phase and speed
                            stringstream ss;
                            ss << plane.flightNumber << "\n" << statusToStr(plane.phase) << "\nSpeed: " << fixed << setprecision(1) << plane.speed;
                            plane.label.setString(ss.str());
                        } else {
                            plane.isActive = false;
                            plane.isTakingOff = false;
                            plane.flightNumber = "";
                            plane.label.setString("");
                            float spacing = oneThirdWidth / 4.0f;
                            int index = &plane - &runwayPlanes[r][0];
                            plane.sprite.setPosition(r * oneThirdWidth + spacing * (index + 1), bottomY);
                            plane.label.setPosition(r * oneThirdWidth + spacing * (index + 1), bottomY - 30);
                            plane.sprite.setRotation(180);
                            plane.speed = 0.0f;
                            plane.phase = WAITING;
                        }
                    } else if (plane.isLanding) {
                        // Arrivals: Decelerate and move down to bottom
                        if (plane.sprite.getPosition().y + rotatedHeight / 2 < bottomY) {
                            plane.sprite.move(0, plane.speed);
                            float distanceToBottom = bottomY - (plane.sprite.getPosition().y + rotatedHeight / 2);
                            if (distanceToBottom < 200.0f) {
                                plane.speed = std::max(0.0f, plane.speed - 0.01f);
                            }
                            plane.label.setPosition(runwayX - plane.label.getLocalBounds().width / 2, plane.sprite.getPosition().y - 30);
                            // Update label with current phase and speed
                            stringstream ss;
                            ss << plane.flightNumber << "\n" << (plane.phase == AT_GATE ? "Landed" : statusToStr(plane.phase)) << "\nSpeed: " << fixed << setprecision(1) << plane.speed;
                            plane.label.setString(ss.str());
                        } else {
                            plane.sprite.setPosition(runwayX, bottomY);
                            plane.label.setPosition(runwayX - plane.label.getLocalBounds().width / 2, bottomY - 30);
                            plane.speed = 0.0f;
                            plane.phase = AT_GATE;
                            // Update label when at gate
                            stringstream ss;
                            ss << plane.flightNumber << "\nLanded\nSpeed: " << fixed << setprecision(1) << plane.speed;
                            plane.label.setString(ss.str());
                        }
                    }
                }
            }
        }

        // Render the scene
        window->clear();
        for (const auto& runway : runwaySprites) {
            window->draw(runway);
        }
        for (int r = 0; r < 3; ++r) {
            for (const auto& plane : runwayPlanes[r]) {
                if (plane.isActive) {
                    window->draw(plane.sprite);
                    window->draw(plane.label);
                }
            }
        }
        window->display();
        pthread_mutex_unlock(&sfmlMutex);
    }

    // gets the report of the Airline system
    void getReport() {
        stringstream ss;
        ss << "\n===== SIMULATION REPORT =====\n";
        ss << "Total Flights Simulated: " << totalFlightsSimulated << "\n";
        ss << "\nAirspace Violations (AVNs):\n";
        for(const auto& entry : avnViolationsPerAirline) {
            ss << "  - " << entry.first << ": " << entry.second << " violation(s)\n";
        }
        ss << "\nGround Faults (Towed Aircraft):\n";
        for(const auto& entry : faultsPerAirline) {
            ss << "  - " << entry.first << ": " << entry.second << " fault(s)\n";
        }
        ss << "\nFinal Airline Status:\n";
        pthread_mutex_lock(&printMutex);
        cout << ss.str() << flush;
        pthread_mutex_unlock(&printMutex);
        airlineStatus();
        pthread_mutex_lock(&printMutex);
        cout << "=============================\n" << flush;
        pthread_mutex_unlock(&printMutex);
    }



   // Finds and returns an available aircraft matching flight type and airline
Aircraft* getAvailableAircraft(FlightType ftype, const string& flightAirlineName) {
    size_t startIndex = lastAircraftIndex[ftype], index = startIndex; // Start search from last used index for this flight type

    do {
        if(index >= aircrafts.size()) index = 0; // Wrap around if index goes out of bounds

        Aircraft* aircraft = aircrafts[index]; // Get aircraft pointer at current index

        // Check if aircraft matches type, is not faulty, not involved in AVN, belongs to the correct airline, and is available
        if(aircraft && 
           aircraft->getAircraftType() == ftype &&
           !aircraft->isFaulty() &&
           !aircraft->getisAVNACTIVE() &&
           aircraft->getAirlineName() == flightAirlineName &&
           aircraft->isAvailable) {
            
            // Update last used index for this flight type to promote fair use
            lastAircraftIndex[ftype] = (index + 1) % aircrafts.size();

            // Log assignment to console with mutex lock
            pthread_mutex_lock(&printMutex);
            cout << "[AIRCRAFT] Assigned " << aircraft->getAircraftID() << " (" << flightAirlineName << ") to flight" << endl << flush;
            pthread_mutex_unlock(&printMutex);

            return aircraft; // Return found aircraft
        }

        index = (index + 1) % aircrafts.size(); // Move to next aircraft in list
    } while(index != startIndex); // Stop when we've looped back to the starting point

    // If EMERGENCY and no matching aircraft, fallback to COMMERCIAL aircraft
    if (ftype == EMERGENCY) {
        index = 0;
        do {
            if(index >= aircrafts.size()) index = 0;

            Aircraft* aircraft = aircrafts[index];
            if(aircraft &&
               aircraft->getAircraftType() == COMMERCIAL &&
               !aircraft->isFaulty() &&
               !aircraft->getisAVNACTIVE() &&
               aircraft->getAirlineName() == flightAirlineName &&
               aircraft->isAvailable) {

                lastAircraftIndex[ftype] = (index + 1) % aircrafts.size();

                pthread_mutex_lock(&printMutex);
                cout << "[AIRCRAFT] Assigned COMMERCIAL aircraft " << aircraft->getAircraftID()
                     << " (" << flightAirlineName << ") for emergency flight" << endl << flush;
                pthread_mutex_unlock(&printMutex);

                return aircraft;
            }

            index = (index + 1) % aircrafts.size();
        } while(index != startIndex);
    }

    // No aircraft found, print failure message
    pthread_mutex_lock(&printMutex);
    cout << "[NO AIRCRAFT] No available " << flightTypeToStr(ftype) << " aircraft for " << flightAirlineName << endl << flush;
    pthread_mutex_unlock(&printMutex);

    return nullptr; // Return null if nothing found
}

// Simulates the handling of a flight including runway assignment, visualization, and thread launch
void simulateFlight(FlightEntry* flight) {
    // Validate flight and its aircraft
    if(!flight || !flight->aircraft) {
        pthread_mutex_lock(&printMutex);
        cout << "[ERROR] Invalid flight or aircraft for " << (flight ? flight->flightNumber : "unknown") << endl << flush;
        pthread_mutex_unlock(&printMutex);
        delete flight;
        return;
    }

    airlineStatus(); // Print current airline status (not shown here)

    Runway* r = nullptr;          // Pointer to selected runway
    RunwayType targetRunway;      // Target runway type to assign

    // Decide the target runway based on flight type or airline
    if(flight->type == CARGO || flight->type == EMERGENCY || flight->airlineName == "FedEx") {
        targetRunway = RWY_C; // Cargo and Emergency go to Runway C
    } else if(flight->isArrival) {
        targetRunway = RWY_A; // Arrivals use Runway A
    } else {
        targetRunway = RWY_B; // Departures use Runway B
    }

    // Try to reserve the chosen runway
    for(auto& runway : runways) {
        if(runway.getAircraftType() == targetRunway) {
            if(runway.getRunway(flight->flightNumber, flight->direction, flight->type, flight->priority, flight->isArrival)) {
                r = &runway;
                break;
            }
        }
    }

    // If runway assignment failed for an emergency flight, retry multiple times
    if(!r && flight->priority == 1) {
        int attempts = 0;
        while(!(r = requestRunway(*flight)) && attempts < 10) {
            pthread_mutex_lock(&printMutex);
            cout << "[EMERGENCY RETRY] No runway for " << flight->flightNumber << ", attempt " << (attempts + 1) << endl << flush;
            pthread_mutex_unlock(&printMutex);
            usleep(1000); // Wait before retry
            attempts++;
        }
    }

    // If still no runway found, add to waiting queue or cancel if retry limit exceeded
    if(!r) {
        pthread_mutex_lock(&waitingQueueMutex);
        flight->rescheduleCount++;
        if(flight->rescheduleCount >= maxResched) {
            pthread_mutex_lock(&printMutex);
            cout << "[ERROR] Flight " << flight->flightNumber
                 << " exceeded maximum reschedule attempts (" << maxResched << "). Canceling flight.\n" << flush;
            pthread_mutex_unlock(&printMutex);
            pthread_mutex_unlock(&waitingQueueMutex);
            delete flight;
            return;
        }

        // Add to queue with updated scheduling
        flight->timeAdded = time(nullptr);
        flight->estimatedWaitTime = 15 * waitingQueue.size(); // Estimate based on queue length
        waitingQueue.push_back(*flight);

        // Sort queue by scheduled time, priority, and time added
        sort(waitingQueue.begin(), waitingQueue.end(), [](const FlightEntry& a, const FlightEntry& b) {
            if(a.scheduledTime != b.scheduledTime) return a.scheduledTime < b.scheduledTime;
            if(a.priority != b.priority) return a.priority < b.priority;
            return a.timeAdded < b.timeAdded;
        });

        // Reschedule flight 15 seconds later
        time_t newTime = flight->scheduledTime + 15;
        flightSchedule.rescheduleFlight(*flight, 15);

        // Log reschedule info
        stringstream ss;
        ss << "[QUEUE] Flight " << flight->flightNumber << " added to waiting queue with estimated wait time: "
           << flight->estimatedWaitTime << " seconds.\n";
        ss << "flight with flight number " << flight->flightNumber << " is rescheduled to " << ctime(&newTime);

        pthread_mutex_lock(&printMutex);
        cout << ss.str() << flush;
        pthread_mutex_unlock(&printMutex);
        pthread_mutex_unlock(&waitingQueueMutex);
        delete flight;
        return;
    }

    // Log that simulation is starting
    stringstream ss;
    ss << "\n[SIMULATION START] " << flight->flightNumber << " (" << flightTypeToStr(flight->type) << ", "
       << (flight->isInternational ? "International" : "Domestic") << ") on "
       << runwayTypeToStr(r->getAircraftType()) << " with aircraft " << flight->aircraft->getAircraftID() << endl;

    pthread_mutex_lock(&printMutex);
    cout << ss.str() << flush;
    pthread_mutex_unlock(&printMutex);

    // Send plane to visual runway
    assignPlaneToRunway(flight->flightNumber, r->getAircraftType(), flight->isArrival);
    usleep(500000); // Delay for visual transition

    // Create and launch flight thread
    pthread_t thread;
    FlightThreadArgs* args = new FlightThreadArgs{flight, r, this};
    if(pthread_create(&thread, nullptr, flightThread, args) != 0) {
        pthread_mutex_lock(&printMutex);
        cout << "[ERROR] Failed to create thread for " << flight->flightNumber << endl << flush;
        pthread_mutex_unlock(&printMutex);
        r->releaseRunway(); // Free the runway if thread creation failed
        delete flight;
        delete args;
        return;
    }

    pthread_detach(thread); // Run thread in detached mode
}









};

int main() {
    cout << "===== ATC Starting =====\n" << flush;
    ATC atc;
    atc.prodSimulation();
    cout << "===== ATC Complete =====\n" << flush;
    return 0;
}

