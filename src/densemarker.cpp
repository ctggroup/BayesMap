#include "densemarker.h"

#include "common.h"
#include "compression.h"

#include <fstream>

double DenseMarker::computeNum(VectorXd &epsilon, const double beta_old)
{
    if (component != 0.0)
        epsilon += beta_old * *Cx;

    return Cx->dot(epsilon);
}

void DenseMarker::updateEpsilon(VectorXd &epsilon, const double beta_old, const double beta)
{
    (void) beta_old; // Unused
    epsilon -= beta * *Cx;
}

CompressedMarker DenseMarker::compress() const
{
    CompressedMarker compressed;
    const auto maxCompressedOutputSize = maxCompressedDataSize<double>(numInds);
    compressed.buffer.reset(new unsigned char[maxCompressedOutputSize]);
    compressed.size = compressData(*Cx,
                                   compressed.buffer.get(),
                                   maxCompressedOutputSize);
    return compressed;
}

void DenseMarker::decompress(unsigned char *data, const IndexEntry &index)
{
    const unsigned int colSize = numInds * sizeof(double);
    buffer.reset(new unsigned char[colSize]);

    extractData(data + index.pos,
                static_cast<unsigned int>(index.size),
                buffer.get(),
                colSize);

    Cx = std::make_shared<Map<VectorXd>>(reinterpret_cast<double *>(buffer.get()),
                                         numInds);
}

void DenseMarker::write(std::ostream *outStream) const
{
    outStream->write(reinterpret_cast<char *>(buffer.get()),
                     Cx->size() * sizeof(double));
}

bool DenseMarker::isValid() const
{
    return buffer && Cx;
}
