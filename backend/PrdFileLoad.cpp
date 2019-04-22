#include "PrdFileLoad.h"

/* Local */
#include "PrdFileUtils.h"

/* System */
#include <cstring>

pm::PrdFileLoad::PrdFileLoad(const std::string& fileName, PrdHeader& header)
    : FileLoad(fileName, header),
    m_file()
{
}

pm::PrdFileLoad::~PrdFileLoad()
{
    if (IsOpen())
        Close();
}

bool pm::PrdFileLoad::Open()
{
    if (IsOpen())
        return true;

    m_file.open(m_fileName, std::ios_base::in | std::ios_base::binary);
    if (!m_file.is_open())
        return false;

    PrdHeader header;
    m_file.read(reinterpret_cast<char*>(&header), sizeof(PrdHeader));
    if (m_file.good() && header.signature == PRD_SIGNATURE)
    {
        m_header = header;

        m_rawDataBytes = GetRawDataSizeInBytes(m_header);
        m_frameIndex = 0;
    }
    else
    {
        Close();
    }

    return IsOpen();
}

bool pm::PrdFileLoad::IsOpen() const
{
    return m_file.is_open();
}

void pm::PrdFileLoad::Close()
{
    m_file.close();
}

bool pm::PrdFileLoad::ReadFrame(const void** metaData, const void** extDynMetaData,
            const void** rawData)
{
    if (!FileLoad::ReadFrame(metaData, extDynMetaData, rawData))
        return false;

    auto ReallocAndRead = [&](void** data, size_t bytes) -> bool
    {
        void* newMem = std::realloc(*data, bytes);
        if (!newMem)
            return false;
        *data = newMem;

        m_file.read(reinterpret_cast<char*>(*data), bytes);
        if (!m_file.good())
            return false;

        return true;
    };

    if (!ReallocAndRead(&m_metaData, m_header.sizeOfPrdMetaDataStruct))
        return false;

    auto prdMetaData = static_cast<PrdMetaData*>(m_metaData);
    if (prdMetaData->extDynMetaDataSize > 0)
    {
        if (!ReallocAndRead(&m_extDynMetaData, prdMetaData->extDynMetaDataSize))
            return false;
    }

    if (!ReallocAndRead(&m_rawData, m_rawDataBytes))
        return false;

    *metaData = m_metaData;
    *extDynMetaData = m_extDynMetaData;
    *rawData = m_rawData;

    return true;
}
