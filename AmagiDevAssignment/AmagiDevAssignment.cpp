#include "AmagiDevAssignment.h"
using namespace std;

/* Contains the border values of an ID interval, as well as the position
inside the binary (from sof in bytes) where the corresponding payload ends*/
class DataInterval {
public:
    unsigned short StartId;
    unsigned short EndingId;
    unsigned long EndingPosition;

    DataInterval(unsigned short start, unsigned short end, unsigned long endPos) {
        StartId = start; EndingId = end; EndingPosition = endPos;
    }
};

/* An instance of this class writes data packet payloads into the binary
   file and keeps track of the packet IDs in memory. */
class BinaryWriter {
public:
    BinaryWriter(const char* fName) {
        fileName = fName;
    }

    void write(const char* data, int size) {
        // ignoring empty packets
        if (size <= 12) return;

        // extracting Id from header
        unsigned short Id = (unsigned short)
            ((unsigned char)data[2] << 8 |
                (unsigned char)data[3]);
        int payloadSize = size - 12;

        // number of intervals (before insertion)
        int n = intervals.size();
        // Index inside "intervals" where the new element needs to be inserted
        int newIdx = binarySearchNew(intervals, 0, n - 1, Id);
        // Offset from the binary file origin (in bytes) where payload needs to
        // be inserted
        int fileOffset = (newIdx > 0) ? intervals[newIdx - 1].EndingPosition : 0;
        // number of bytes inside the binary file right of "fileOffset". These
        // bytes need to be copied to a dummy file and returned after the
        // payload is inserted in its place.
        int tailLength = (n > 0) ? intervals[n - 1].EndingPosition - fileOffset : 0;

        fstream file;
        fstream auxiliary;
        file.open(fileName, ios::binary | ios::out | ios::in);
        // if the file does not exist
        if (!file.is_open()) {
            file.open(fileName, ios::binary | ios::out);
            file.write(&data[12], payloadSize);
            file.close();
            UpdateIntervals(Id, payloadSize, n, newIdx);
            return;
        }
        // "temporary.dat" is the dummy file for copying
        auxiliary.open("temporary.dat", ios::binary | ios::out);

        // coopying into temporary.dat
        BinaryWriter::CopyPartOfFile(file, auxiliary, fileOffset, 0, tailLength);
        auxiliary.close();

        // inserting payload and copying back from temporary.dat
        auxiliary.open("temporary.dat", ios::binary | ios::in);
        file.seekp(fileOffset);
        file.write(&data[12], payloadSize);
        BinaryWriter::CopyPartOfFile(auxiliary, file, 0, fileOffset + payloadSize, tailLength);

        auxiliary.close();
        file.close();

        // updating the "intervals" vector for bookkeeping
        UpdateIntervals(Id, payloadSize, n, newIdx);
    }

private:
    /* holds the information about the data packets stored in the file */
    vector<DataInterval> intervals = {};
    const char* fileName;

    /* Updates the intervals */
    void UpdateIntervals(int Id, int size, int n, int newIdx) {
        // the new entry may be merged with its neighbours if their 
        // indices differ by 1.
        // 
        // merging condition.
        // 0 -> no merge
        // 1 -> left merge
        // 2 -> right merge
        // 3 -> left and right merge
        int mergeCond = 0;
        if (newIdx > 0) {
            if (intervals[newIdx - 1].EndingId == Id - 1) mergeCond += 1;
        }
        if (newIdx < n) {
            if (intervals[newIdx].StartId == Id + 1) mergeCond += 2;
        }

        // merging and inserting
        // no merge
        if (mergeCond == 0) {
            DataInterval newInterval(Id, Id, 
                (newIdx == 0) ? size : size + intervals[newIdx - 1].EndingPosition);
            
            intervals.insert(intervals.begin() + newIdx, newInterval);
        }
        // left merge
        else if (mergeCond == 1) {
            intervals[newIdx - 1].EndingPosition += size;
            intervals[newIdx - 1].EndingId = Id;
        }
        // right merge
        else if (mergeCond == 2) {
            intervals[newIdx].EndingPosition += size;
            intervals[newIdx].StartId = Id;
        }
        // left and right merge
        else {
            intervals[newIdx].EndingPosition += size;
            intervals[newIdx].StartId = intervals[newIdx - 1].StartId;
            intervals.erase(intervals.begin() + newIdx - 1);
        }

        int low, upp;
        switch (mergeCond) {
        case 0:
            low = newIdx + 1; upp = n + 1; break;
        case 1:
            low = newIdx; upp = n; break;
        case 2:
            low = newIdx + 1; upp = n; break;
        case 3:
            low = newIdx; upp = n - 1; break;
        }

        // updating intervals to the right of the merged region
        for (int i = low; i < upp; i++) {
            intervals[i].EndingPosition += size;
        }
    }

    /* returns the index (in the intervals vector) where the new element
    should be placed. It is assumed that the element's ID does not overlap
    with previously seen IDs (raises exception if that happens) */
    int binarySearchNew(vector<DataInterval> arr, int low, int high, int Id)
    {
        while (low <= high) {
            int mid = low + (high - low) / 2;

            if (arr[mid].StartId == Id)
                // this should not happen if indices are unique
                throw std::invalid_argument("packet ID already exists");

            if (arr[mid].StartId < Id)
                low = mid + 1;

            else
                high = mid - 1;
        }

        // returns index at which the new object needs to be inserted
        return low;
    }

    /* Helper method. Copies part of binary file (from readFromOffset) with length
    "length" to other file (from writeFromOffset). The process is done through a buffer
    in chunks of BUFFER_SIZE */
    static void CopyPartOfFile(fstream& readingStream, fstream& writingStream,
        unsigned long readFromOffset, unsigned long writeFromOffset, unsigned long lengthToCopy) {
        const int BUFFER_SIZE = 512;
        char buffer[BUFFER_SIZE];

        readingStream.seekg(readFromOffset);
        int remainder = lengthToCopy % BUFFER_SIZE;

        writingStream.seekp(writeFromOffset);
        for (int i = 0; i < lengthToCopy / BUFFER_SIZE; i++) {
            readingStream.read(buffer, BUFFER_SIZE);
            writingStream.write(buffer, BUFFER_SIZE);
        }
        readingStream.read(buffer, remainder);
        writingStream.write(buffer, remainder);
    }
};


/* Auxiliary function for testing. Prints the current contents of the
binary file as an unsigned char sequence. */
void PrintState(const char* fName) {
    fstream file;
    file.open(fName, ios::binary | ios::in);
    char fullBuffer[1024]; // for simplicity, it is assumed that the bin 
                           // file used in the test is shorter than 1024
                           // (this is an arbitrarily chosen number)
    file.seekg(0, SEEK_END);
    int length = file.tellg();
    file.seekg(0);
    file.read(fullBuffer, length);
    file.close();

    cout << "\nFILE CONTENT:\n";
    for (int i = 0; i < length; i++) {
        cout << (unsigned char)fullBuffer[i];
    }
    cout << "\n";
}

int main()
{
    // a mock example of incoming packets (z-s denote the irrelevant 
    // header parts)
    const char* testArray[20]{
        "zz\x01\x61zzzzzzzzaaa",
        "zz\x01\x62zzzzzzzzbb",
        "zz\x01\x63zzzzzzzzccc",
        "zz\x01\x64zzzzzzzzdddd",
        "zz\x01\x65zzzzzzzzeeeeee",
        "zz\x01\x66zzzzzzzzfffff",
        "zz\x01\x67zzzzzzzzgggggggggg",
        "zz\x01\x68zzzzzzzzh",
        "zz\x01\x69zzzzzzzziiiiiiiiiiiiiii",
        "zz\x01\x6Azzzzzzzzjjjj",
        "zz\x01\x6Bzzzzzzzzkkkkkk",
        "zz\x01\x6Czzzzzzzzlll",
        "zz\x01\x6Dzzzzzzzzmm",
        "zz\x01\x6Ezzzzzzzznnnn",
        "zz\x01\x6Fzzzzzzzzoooooo",
        "zz\x01\x70zzzzzzzzppp",
        "zz\x01\x71zzzzzzzzqqqqq",
        "zz\x01\x72zzzzzzzzr",
        "zz\x01\x73zzzzzzzzsssssssssssssssssssss",
        "zz\x01\x74zzzzzzzzttt"
    };
    // the incoming order is randomly shuffled
    auto rng = std::default_random_engine{};
    srand(time(0));
    rng.seed(rand());
    std::shuffle(begin(testArray), end(testArray), rng);

    // initializing writer
    const char* OUTPUT_FILE = "results.dat";
    BinaryWriter writer(OUTPUT_FILE);

    // writing into binary file
    for (int i = 0; i < 20; i++) {
        cout << testArray[i] << "\n";
        writer.write(testArray[i], strlen(testArray[i]));
    }

    // printing the contents of the binary file
    PrintState(OUTPUT_FILE);

    // clearing the binary file, done for testing purposes.
    /*
    fstream file;
    file.open(OUTPUT_FILE, ios::binary | ios::out);
    file.close();
    */
}
