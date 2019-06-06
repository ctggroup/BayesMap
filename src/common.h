#ifndef COMMON_H
#define COMMON_H

#include <string>

enum InputType : unsigned int {
    Unknown = 0,
    BED,
    CSV
};

enum DataType : unsigned int {
    None = 0,
    Dense,
    SparseEigen,
    SparseRagged
};

class MarkerBuilder;
MarkerBuilder* builderForType(const DataType type);

std::string ppFileForType(DataType type, const std::string &bedFile);
std::string ppIndexFileForType(DataType type, const std::string &bedFile);

InputType getInputType(const std::string &dataFile);

// An entry for the index to the compressed preprocessed bed file
struct IndexEntry {
    unsigned long pos = 0;
    unsigned long compressedSize = 0;
    unsigned long originalSize = 0;
};

//template to read csv files into an eigen vector.
template<typename M>
M load_csv (const std::string & path);


#endif // COMMON_H
