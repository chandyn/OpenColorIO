// SPDX-License-Identifier: BSD-3-Clause
// Copyright Contributors to the OpenColorIO Project.

#include "fileformats/amf/AMFParser.cpp"

#include "testutils/UnitTest.h"
#include "UnitTestUtils.h"

namespace OCIO_NAMESPACE
{
	OCIO_ADD_TEST(AMFParser, CreateFromAMF)
	{
		AMFInfoRcPtr amfInfoObject = std::make_shared<AMFInfo>();
		std::string amfFilePath(GetTestFilesDir() + "/amf/example.amf");
		ConstConfigRcPtr amfConfig = CreateFromAMF(amfInfoObject, amfFilePath.c_str());
		OCIO_CHECK_NO_THROW(amfConfig->validate());

		amfFilePath = GetTestFilesDir() + "/amf/example4.amf";
		amfConfig = CreateFromAMF(amfInfoObject, amfFilePath.c_str());
		OCIO_CHECK_NO_THROW(amfConfig->validate());
	}

} // namespace OCIO_NAMESPACE
