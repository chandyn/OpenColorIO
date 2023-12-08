// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the OpenColorIO Project.

#ifndef INCLUDED_OCIO_FILEFORMATS_AMF_AMFPARSER_H
#define INCLUDED_OCIO_FILEFORMATS_AMF_AMFPARSER_H

#include <OpenColorIO/OpenColorIO.h>

namespace OCIO_NAMESPACE
{

class AMFParser
{
public:
    explicit AMFParser() : m_impl(NULL) {};
    virtual ~AMFParser()
    {
        if (m_impl == NULL)
            return;

        delete m_impl;
        m_impl = NULL;
    }

    ConstConfigRcPtr buildConfig(AMFInfoRcPtr amfInfoObject, const char* amfFilePath);

private:
    class Impl;
    Impl* m_impl; 

    AMFParser(const AMFParser&) = delete;
    AMFParser& operator=(const AMFParser&) = delete;
};

} // namespace OCIO_NAMESPACE

#endif
