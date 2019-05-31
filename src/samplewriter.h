#ifndef SAMPLEWRITER_H
#define SAMPLEWRITER_H

#include <Eigen/Eigen>

#include <fstream>
#include <string>

class SampleWriter
{
public:
    SampleWriter();
    ~SampleWriter();

    void setFileName(const std::string &fileName) { m_fileName = fileName; }
    std::string fileName() const { return m_fileName; }

    void setMarkerCount(unsigned int markerCount) { m_markerCount = markerCount; }
    unsigned int markerCount() const { return m_markerCount; }

    void setIndividualCount(unsigned int individualCount) { m_individualCount = individualCount; }
    unsigned int individualCount() const { return m_individualCount; }

    void setFixedCount(unsigned int fixedCount) { m_fixedCount = fixedCount; }
    unsigned int fixedCount() const { return m_fixedCount; }

    void open();
    void open_bayesW();
    void open_bayesW_fixed();


    void write(const Eigen::VectorXd &sample);
    void close();

private:
    std::string m_fileName;
    std::ofstream m_outFile;
    unsigned int m_markerCount;
    unsigned int m_individualCount;
    unsigned int m_fixedCount;
    Eigen::IOFormat m_commaInitFormat;
};

#endif // SAMPLEWRITER_H
