#include "data.hpp"
#include <Eigen/Eigen>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <iterator>
#include "compression.h"

#define handle_error(msg)                               \
    do { perror(msg); exit(EXIT_FAILURE); } while (0)

Data::Data()
    : ppBedFd(-1)
    , ppBedMap(nullptr)
    , mappedZ(nullptr, 1, 1)
    , ppbedIndex()
{
}

#ifdef USE_MPI

// Read raw data loaded in memory to preprocess them (center, scale and cast to double)
void Data::preprocess_data(const char* rawdata, const uint NC, const uint NB, double* ppdata, const int rank) {

    assert(numInds<=NB*4);

    for (int i=0; i<NC; ++i) {

        char* locraw = (char*)&rawdata[i*NB];

        // temporary array used for translation
        int8_t *tmpi = (int8_t*)malloc(NB * 4 * sizeof(int8_t));

        for (int ii=0; ii<NB; ++ii) {
            for (int iii=0; iii<4; ++iii) {
                tmpi[ii*4 + iii] = (locraw[ii] >> 2*iii) & 0b11;   // ADJUST ON RANK & NC
            }
        }
        
        for (int ii=0; ii<numInds; ++ii) {
            if (tmpi[ii] == 1) {
                tmpi[ii] = -1;
            } else {
                tmpi[ii] =  2 - ((tmpi[ii] & 0b1) + ((tmpi[ii] >> 1) & 0b1));
            }
        }

        int sum = 0, nmiss = 0;
        //#pragma omp simd reduction(+:sum) reduction(+:nmiss)
        for (int ii=0; ii<numInds; ++ii) {
            if (tmpi[ii] < 0) {
                nmiss += tmpi[ii];
            } else {
                sum   += tmpi[ii];
            }
        }

        double mean = double(sum) / double(numInds + nmiss); //EO: nmiss is neg
        //printf("rank %d snpInd %2d: sum = %6d, N = %6d, nmiss = %6d, mean = %20.15f\n",
        //       rank, rank*NC+i, sum, numKeptInds, nmiss, mean);

        size_t ppdata_i = size_t(i) * size_t(numInds);
        double *locpp = (double*)&ppdata[ppdata_i];

        for (size_t ii=0; ii<numInds; ++ii) {
            if (tmpi[ii] < 0) {
                locpp[ii] = 0.0d;
            } else {
                locpp[ii] = double(tmpi[ii]) - mean;
            }
        }

        double sqn  = 0.0d;
        for (size_t ii=0; ii<numInds; ++ii) {
            sqn += locpp[ii] * locpp[ii];
        }

        double std_ = sqrt(double(numInds - 1) / sqn);

        for (size_t ii=0; ii<numInds; ++ii) {
            locpp[ii] *= std_;
        }

        free(tmpi);
    }
}
#endif



void Data::preprocessBedFile(const string &bedFile, const string &preprocessedBedFile, const string &preprocessedBedIndexFile, bool compress)
{
    cout << "Preprocessing bed file: " << bedFile << ", Compress data = " << (compress ? "yes" : "no") << endl;
    if (numSnps == 0)
        throw ("Error: No SNP is retained for analysis.");
    if (numInds == 0)
        throw ("Error: No individual is retained for analysis.");

    ifstream BIT(bedFile.c_str(), ios::binary);
    if (!BIT)
        throw ("Error: can not open the file [" + bedFile + "] to read.");

    ofstream ppBedOutput(preprocessedBedFile.c_str(), ios::binary);
    if (!ppBedOutput)
        throw("Error: Unable to open the preprocessed bed file [" + preprocessedBedFile + "] for writing.");
    ofstream ppBedIndexOutput(preprocessedBedIndexFile.c_str(), ios::binary);
    if (!ppBedIndexOutput)
        throw("Error: Unable to open the preprocessed bed index file [" + preprocessedBedIndexFile + "] for writing.");

    cout << "Reading PLINK BED file from [" + bedFile + "] in SNP-major format ..." << endl;
    char header[3];
    BIT.read(header, 3);
    if (!BIT || header[0] != 0x6c || header[1] != 0x1b || header[2] != 0x01)
        throw ("Error: Incorrect first three bytes of bed file: " + bedFile);

    // How much space do we need to compress the data (if requested)
    const auto maxCompressedOutputSize = compress ? maxCompressedDataSize(numInds) : 0;
    unsigned char *compressedBuffer = nullptr;
    unsigned long pos = 0;
    if (compress)
        compressedBuffer = new unsigned char[maxCompressedOutputSize];

    // Read genotype in SNP-major mode, 00: homozygote AA; 11: homozygote BB; 10: hetezygote; 01: missing
    for (unsigned int j = 0, snp = 0; j < numSnps; j++) {
        SnpInfo *snpInfo = snpInfoVec[j];
        double sum = 0.0;
        unsigned int nmiss = 0;

        // Create some scratch space to preprocess the raw data
        VectorXd snpData(numInds);

        // Make a note of which individuals have a missing genotype
        vector<long> missingIndices;

        const unsigned int size = (numInds + 3) >> 2;
        if (!snpInfo->included) {
            BIT.ignore(size);
            continue;
        }

        for (unsigned int i = 0, ind = 0; i < numInds;) {
            char ch;
            BIT.read(&ch, 1);
            if (!BIT)
                throw ("Error: problem with the BED file ... has the FAM/BIM file been changed?");

            bitset<8> b = ch;
            unsigned int k = 0;

            while (k < 7 && i < numInds) {
                if (!indInfoVec[i]->kept) {
                    k += 2;
                } else {
                    const unsigned int allele1 = (!b[k++]);
                    const unsigned int allele2 = (!b[k++]);

                    if (allele1 == 0 && allele2 == 1) {  // missing genotype
                        // Don't store a marker value like this as it requires floating point comparisons later
                        // which are not done properly. Instead, store the index of the individual in a vector and simply
                        // iterate over the collected indices. Also means iterating over far fewer elements which may
                        // make a noticeable difference as this scales up.
                        missingIndices.push_back(ind++);
                        ++nmiss;
                    } else {
                        const auto value = allele1 + allele2;
                        snpData[ind++] = value;
                        sum += value;
                    }
                }
                i++;
            }
        }

        // Fill missing values with the mean
        const double mean = sum / double(numInds - nmiss);
        if (j % 100 == 0) {
            printf("MARKER %6d mean = %12.7f computed on %6.0f with %6d elements (%d - %d)\n",
                   j, mean, sum, numInds-nmiss, numInds, nmiss);
            fflush(stdout);
        }
        if (nmiss) {
            for (const auto index : missingIndices)
                snpData[index] = mean;
        }

        // Standardize genotypes
        snpData.array() -= snpData.mean();
        const auto sqn = snpData.squaredNorm();
        const auto sigma = 1.0 / (sqrt(sqn / (double(numInds - 1))));
        snpData.array() *= sigma;

        // Write out the preprocessed data
        if (!compress) {
            ppBedOutput.write(reinterpret_cast<char *>(&snpData[0]), numInds * sizeof(double));
        } else {
            const unsigned long compressedSize = compressData(snpData, compressedBuffer, maxCompressedOutputSize);
            ppBedOutput.write(reinterpret_cast<char *>(compressedBuffer), long(compressedSize));

            // Calculate the index data for this column
            ppBedIndexOutput.write(reinterpret_cast<char *>(&pos), sizeof(unsigned long));
            ppBedIndexOutput.write(reinterpret_cast<const char *>(&compressedSize), sizeof(unsigned long));
            pos += compressedSize;
        }

        // Compute allele frequency and any other required data and write out to file
        //snpInfo->af = 0.5f * float(mean);
        //snp2pq[snp] = 2.0f * snpInfo->af * (1.0f - snpInfo->af);

        if (++snp == numSnps)
            break;
    }

    if (compress)
        delete[] compressedBuffer;

    BIT.clear();
    BIT.close();

    cout << "Genotype data for " << numInds << " individuals and " << numSnps << " SNPs are included from [" + bedFile + "]." << endl;
}

void Data::mapPreprocessBedFile(const string &preprocessedBedFile)
{
    // Calculate the expected file sizes - cast to size_t so that we don't overflow the unsigned int's
    // that we would otherwise get as intermediate variables!
    const size_t ppBedSize = size_t(numInds) * size_t(numSnps) * sizeof(double);

    // Open and mmap the preprocessed bed file
    ppBedFd = open(preprocessedBedFile.c_str(), O_RDONLY);
    if (ppBedFd == -1)
        throw("Error: Failed to open preprocessed bed file [" + preprocessedBedFile + "]");

    ppBedMap = reinterpret_cast<double *>(mmap(nullptr, ppBedSize, PROT_READ, MAP_SHARED, ppBedFd, 0));
    if (ppBedMap == MAP_FAILED)
        throw("Error: Failed to mmap preprocessed bed file");

    // Now that the raw data is available, wrap it into the mapped Eigen types using the
    // placement new operator.
    // See https://eigen.tuxfamily.org/dox/group__TutorialMapClass.html#TutorialMapPlacementNew
    new (&mappedZ) Map<MatrixXd>(ppBedMap, numInds, numSnps);
}

void Data::unmapPreprocessedBedFile()
{
    // Unmap the data from the Eigen accessors
    new (&mappedZ) Map<MatrixXd>(nullptr, 1, 1);

    const auto ppBedSize = numInds * numSnps * sizeof(double);
    munmap(ppBedMap, ppBedSize);
    close(ppBedFd);
}

void Data::mapCompressedPreprocessBedFile(const string &preprocessedBedFile,
                                          const string &indexFile)
{
    // Load the index to the compressed preprocessed bed file
    ppbedIndex.resize(numSnps);
    ifstream indexStream(indexFile, std::ifstream::binary);
    if (!indexStream)
        throw("Error: Failed to open compressed preprocessed bed file index");
    indexStream.read(reinterpret_cast<char *>(ppbedIndex.data()),
                     numSnps * 2 * sizeof(long));

    // Calculate the expected file sizes - cast to size_t so that we don't overflow the unsigned int's
    // that we would otherwise get as intermediate variables!
    const size_t ppBedSize = size_t(ppbedIndex.back().pos + ppbedIndex.back().size);

    // Open and mmap the preprocessed bed file
    ppBedFd = open(preprocessedBedFile.c_str(), O_RDONLY);
    if (ppBedFd == -1)
        throw("Error: Failed to open preprocessed bed file [" + preprocessedBedFile + "]");

    ppBedMap = reinterpret_cast<double *>(mmap(nullptr, ppBedSize, PROT_READ, MAP_SHARED, ppBedFd, 0));
    if (ppBedMap == MAP_FAILED)
        throw("Error: Failed to mmap preprocessed bed file");
}

void Data::unmapCompressedPreprocessedBedFile()
{
    const size_t ppBedSize = size_t(ppbedIndex.back().pos + ppbedIndex.back().size);
    munmap(ppBedMap, ppBedSize);
    close(ppBedFd);
    ppbedIndex.clear();
}

void Data::readFamFile(const string &famFile){
    // ignore phenotype column
    ifstream in(famFile.c_str());
    if (!in) throw ("Error: can not open the file [" + famFile + "] to read.");
#ifndef USE_MPI
    cout << "Reading PLINK FAM file from [" + famFile + "]." << endl;
#endif
    indInfoVec.clear();
    indInfoMap.clear();
    string fid, pid, dad, mom, sex, phen;
    unsigned idx = 0;
    while (in >> fid >> pid >> dad >> mom >> sex >> phen) {
        IndInfo *ind = new IndInfo(idx++, fid, pid, dad, mom, atoi(sex.c_str()));
        indInfoVec.push_back(ind);
        if (indInfoMap.insert(pair<string, IndInfo*>(ind->catID, ind)).second == false) {
            throw ("Error: Duplicate individual ID found: \"" + fid + "\t" + pid + "\".");
        }
    }
    in.close();
    numInds = (unsigned) indInfoVec.size();
#ifndef USE_MPI
    cout << numInds << " individuals to be included from [" + famFile + "]." << endl;
#endif
}

void Data::readBimFile(const string &bimFile) {
    // Read bim file: recombination rate is defined between SNP i and SNP i-1
    ifstream in(bimFile.c_str());
    if (!in) throw ("Error: can not open the file [" + bimFile + "] to read.");
#ifndef USE_MPI
    cout << "Reading PLINK BIM file from [" + bimFile + "]." << endl;
#endif
    snpInfoVec.clear();
    snpInfoMap.clear();
    string id, allele1, allele2;
    unsigned chr, physPos;
    float genPos;
    unsigned idx = 0;
    while (in >> chr >> id >> genPos >> physPos >> allele1 >> allele2) {
        SnpInfo *snp = new SnpInfo(idx++, id, allele1, allele2, chr, genPos, physPos);
        snpInfoVec.push_back(snp);
        if (snpInfoMap.insert(pair<string, SnpInfo*>(id, snp)).second == false) {
            throw ("Error: Duplicate SNP ID found: \"" + id + "\".");
        }
    }
    in.close();
    numSnps = (unsigned) snpInfoVec.size();
#ifndef USE_MPI
    cout << numSnps << " SNPs to be included from [" + bimFile + "]." << endl;
#endif
}


void Data::readBedFile_noMPI(const string &bedFile){
    unsigned i = 0, j = 0, k = 0;

    if (numSnps == 0) throw ("Error: No SNP is retained for analysis.");
    if (numInds == 0) throw ("Error: No individual is retained for analysis.");

    Z.resize(numInds, numSnps);
    ZPZdiag.resize(numSnps);
    snp2pq.resize(numSnps);

    // Read bed file
    char ch[1];
    bitset<8> b;
    unsigned allele1=0, allele2=0;
    ifstream BIT(bedFile.c_str(), ios::binary);
    if (!BIT) throw ("Error: can not open the file [" + bedFile + "] to read.");
#ifndef USE_MPI
    cout << "Reading PLINK BED file from [" + bedFile + "] in SNP-major format ..." << endl;
#endif
    for (i = 0; i < 3; i++) BIT.read(ch, 1); // skip the first three bytes
    SnpInfo *snpInfo = NULL;
    unsigned snp = 0, ind = 0;
    unsigned nmiss = 0;
    float mean = 0.0;

    for (j = 0, snp = 0; j < numSnps; j++) { // Read genotype in SNP-major mode, 00: homozygote AA; 11: homozygote BB; 10: hetezygote; 01: missing
        snpInfo = snpInfoVec[j];
        mean = 0.0;
        nmiss = 0;
        if (!snpInfo->included) {
            for (i = 0; i < numInds; i += 4) BIT.read(ch, 1);
            continue;
        }
        for (i = 0, ind = 0; i < numInds;) {
            BIT.read(ch, 1);
            if (!BIT) throw ("Error: problem with the BED file ... has the FAM/BIM file been changed?");
            b = ch[0];
            k = 0;
            while (k < 7 && i < numInds) {
                if (!indInfoVec[i]->kept) k += 2;
                else {
                    allele1 = (!b[k++]);
                    allele2 = (!b[k++]);
                    if (allele1 == 0 && allele2 == 1) {  // missing genotype
                        Z(ind++, snp) = -9;
                        ++nmiss;
                    } else {
                        mean += Z(ind++, snp) = allele1 + allele2;
                    }
                }
                i++;
            }
        }
        // fill missing values with the mean
        float sum = mean;
        mean /= float(numInds-nmiss);

        if (nmiss) {
            for (i=0; i<numInds; ++i) {
                if (Z(i,snp) == -9) Z(i,snp) = mean;
            }
        }

        // compute allele frequency
        snpInfo->af = 0.5f*mean;
        snp2pq[snp] = 2.0f*snpInfo->af*(1.0f-snpInfo->af);

        // standardize genotypes
        Z.col(j).array() -= mean;

        float sqn = Z.col(j).squaredNorm();
        float std_ = 1.f / (sqrt(sqn / float(numInds)));
        Z.col(j).array() *= std_;

        ZPZdiag[j] = sqn;

        if (++snp == numSnps) break;
    }
    BIT.clear();
    BIT.close();
    // standardize genotypes
    for (i=0; i<numSnps; ++i) {
        Z.col(i).array() -= Z.col(i).mean();
        ZPZdiag[i] = Z.col(i).squaredNorm();
    }
#ifndef USE_MPI
    cout << "Genotype data for " << numInds << " individuals and " << numSnps << " SNPs are included from [" + bedFile + "]." << endl;
#endif
}

void Data::readPhenotypeFile(const string &phenFile) {
    // NA: missing phenotype
    ifstream in(phenFile.c_str());
    if (!in) throw ("Error: can not open the phenotype file [" + phenFile + "] to read.");
#ifndef USE_MPI
    cout << "Reading phenotypes from [" + phenFile + "]." << endl;
#endif
    map<string, IndInfo*>::iterator it, end=indInfoMap.end();
    IndInfo *ind = NULL;
    Gadget::Tokenizer colData;
    string inputStr;
    string sep(" \t");
    string id;
    unsigned line=0;
    //correct loop to go through numInds
    y.setZero(numInds);

    while (getline(in,inputStr)) {
        colData.getTokens(inputStr, sep);
        id = colData[0] + ":" + colData[1];
        it = indInfoMap.find(id);
        // First one corresponded to mphen variable (1+1)
        if (it != end && colData[1+1] != "NA") {
            ind = it->second;
            ind->phenotype = atof(colData[1+1].c_str());
            //fill in phenotype y vector
            y[line] = ind->phenotype;
            ++line;
        }
    }

    in.close();
}
//TODO Finish function to read the group file
void Data::readGroupFile(const string &groupFile) {
    // NA: missing phenotype
    ifstream in(groupFile.c_str());
    if (!in) throw ("Error: can not open the group file [" + groupFile + "] to read.");

    cout << "Reading groups from [" + groupFile + "]." << endl;

    std::istream_iterator<double> start(in), end;
    std::vector<int> numbers(start, end);
    int* ptr =(int*)&numbers[0];
    G=(Eigen::Map<Eigen::VectorXi>(ptr,numbers.size()));

    cout << "Groups read from file" << endl;
}
