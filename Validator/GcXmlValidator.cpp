////////////////////////////////////////////////////////////////////////////////
// File: GcXmlValidator.cpp
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Camera Gen<i>Cam XML validation utility
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Author: Nikolay Bitkin
// Created:	06-JUNE-2018
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// Copyright (C) 2018-2019 Imperx, Inc. All rights reserved. 
////////////////////////////////////////////////////////////////////////////////

#include "stdafx.h"

//! \brief Utility for camera Gen<i>Cam XML file validation 
//! 
//! Usage:
//!  GcXmlValidator.exe cameraxml_name <output_file_name>.
//!  If <output_file_name> was not set, errors to be output to STDIO


using namespace GENICAM_NAMESPACE;
using namespace GENAPI_NAMESPACE;
using namespace std;

// Function ptrototypes:
string FindXmlErrorLineStr(GenericException &ex, const char *fileName);
string FindSwissKnifeErrorLineStr(const char *fileName, const char *strError);
bool GetLineNumbers(const char *fileName, const char *strFind, vector<uint32_t> &lines);

// Usage:
// GcXmlValidator.exe cameraxml_name <output_file_name>
// If <output_file_name> was not set, errors to be output to STDIO
//
int main(int num, char *argc[])
{
	// Return value: 0 - OK, >0 - error
	int retError = 0;
	
	// Validate input data
	if (num < 2)
	{
		cout << "Usage: GcXmlValidator.exe cameraxml_name <output_file_name>\n";
		return 1;
	}

	// Output file
	ofstream outFile;

	// Check if output_file_name was set, and try open it.
	if (num >= 3)
	{
		string strOutFile(argc[2]);
		outFile.open(strOutFile, ios::out);
		//
		if (!outFile.is_open())
			cout << "WARNING: can not open output_file" << strOutFile << "cout will be used\n";
	}

	
	// Output stream is cout or file if specified and opened
	ostream &outStream = (outFile.is_open()) ? static_cast<ostream&>(outFile) : cout;

	// Create the empty nodemap
	unique_ptr<GenApi::CNodeMapRef> nodeMap;
	nodeMap.reset(new GenApi::CNodeMapRef("TestXmlDevice"));

	// Camera XML file name
	gcstring strFile(argc[1]);

	try 
	{
		// Try load the camera XML file 
		nodeMap->_LoadXMLFromFile(strFile);

		// Parse the swiss knifes and collect the errors
		gcstring_vector strErrors;
		nodeMap->_ParseSwissKnifes(&strErrors);
		for (auto iErr : strErrors)
		{
			// Error text
			const char *strFormulaErr = iErr.c_str();

			// Find error line number
			string strErr = FindSwissKnifeErrorLineStr(strFile.c_str(), strFormulaErr);

			// Ouput error data
			outStream << strFile << "(" << strErr << "): error ParseSwissKnifes: " << strFormulaErr << "\n";
			retError = 2;
		}
	}
	catch (GenericException &ex)
	{
		// GenICam error
		const char *strDesc = ex.GetDescription();
		const char *strWhat = ex.what();

		// Find error line number
		string strLineNum = FindXmlErrorLineStr(ex, strFile.c_str());

		// Ouput error data
		outStream << strFile << "(" << strLineNum << "): error GenericException: " << strWhat << "\n";
		retError = 3;
	}
	catch (...)
	{
		// Unknown error (may be crash)
		outStream << strFile << "(1): error Unknown: " << "Unknown, LoadXMLFromFile() failed\n";
		retError = 4;
	}

	// No errors
	if (!retError)
	{
		outStream << strFile << " successfully validated by Gen<i>Cam Version "
			<< GENICAM_VERSION_MAJOR_STR << "."
			<< GENICAM_VERSION_MINOR_STR << "."
			<< GENICAM_VERSION_SUBMINOR_STR << "."
			<< GENICAM_VERSION_BUILD_STR << "\n";
	}
		
	// Close file if opened
	if (!outFile.is_open())
		outFile.close();

    return retError;
}

// Find the XML file line number, where error occured
string FindXmlErrorLineStr(GenericException &ex, const char *fileName)
{
	string strLineN("1"); // set to "1" by default

	// Source info
	int srcLine = ex.GetSourceLine();
	const char *srcName = ex.GetSourceFileName();

	// Get Exception info data
	const char *csDesc = ex.GetDescription();
	const char *csWhat = ex.what();
	string strDesc(csDesc); // use this ...

	//
	// Sample code from "source\GenApi\src\XmlParser\XmlParser.cpp", "source\GenApi\src\NodeMapData\NodeDataMap.cpp":
	//
	//	1) Error line number explicitly mentioned in string:
	//      throw RUNTIME_EXCEPTION("Error while parsing XML stream at line %lu and column %lu : '%s'",
	//		throw GenICam::ExceptionReporter ... ("XML Parse error at line %d and column %d, %s"
	// 
	//	2) Node name mentioned - file to be searched for this name
	//      throw RUNTIME_EXCEPTION("Error in XML stream : dangling node reference '%s'",
	//		throw RUNTIME_EXCEPTION("Fatal error : Dangling node reference '%s'", NodeName.c_str());
	//		throw RUNTIME_EXCEPTION("Found a duplicate node'%s'."
	//		throw RUNTIME_EXCEPTION("Merge conflict with node '%s'"
	//		throw INVALID_ARGUMENT_EXCEPTION("Node '%s' not found"
	//  3) Misc...
	//		throw PROPERTY_EXCEPTION("Error in property of type '%hs': cannot convert '%hs' to int64_t",
	//
	// Reference strings of type 1
	vector<string> strRef1 = { 
		"Error while parsing XML stream at line ", 
		"XML Parse error at line " };
	// Reference strings of type 2
	vector<string> strRef2 = {
		"Error in XML stream : dangling node reference ", 
		"Fatal error : Dangling node referenc e",
		"Found a duplicate node",
		"Merge conflict with node ",
		"Node ",
	};

	//
	size_t idx=0;
	// Find reference strings of type 1, and extract the line number
	for (auto istr : strRef1)
	{
		idx = strDesc.find(istr.c_str());
		if (string::npos != idx)
		{
			// Erase reference string from the error description string
			strDesc.erase(0, istr.length());

			// Truncate characters after first space, including the space 
			idx = strDesc.find(" ");
			if (string::npos != idx)
			{
				strDesc.erase(idx, string::npos);
				strLineN = strDesc;
				// Line number found!
				return strLineN;
			}
		}
	}

	// Find reference strings of type 2,  find the node name in file, and get the line number
	for (auto istr : strRef2)
	{
		idx = strDesc.find(istr.c_str());
		if (string::npos != idx)
		{
			// Parse the string to find the node name in quotes
			size_t n1 = strDesc.find("\'");
			size_t n2 = strDesc.find("\'", n1 + 1);
			if (n1 != string::npos && n2 != string::npos && n1 < n2)
			{
				strDesc.erase(0, n1+1);
				strDesc.erase(n2-n1-1, string::npos);

				vector<uint32_t> lineNums;
				if (GetLineNumbers(fileName, strDesc.c_str(), lineNums) && lineNums.size()>0)
				{
					// We actually need only first line number... me be in future we will use all found
					strLineN = to_string(lineNums[0]);
					// Line number found!
					return strLineN;
				}
				
			}
		}
	}

	// Find reference strings of type 3,  find the node name in file, and get the line number
	idx = strDesc.find("Error in property of type ");
	if (string::npos != idx)
	{
		// Parse the string to find the node name in quotes
		size_t n1 = strDesc.find("\'");
		size_t n2 = strDesc.find("\'", n1 + 1);
		size_t n3 = strDesc.find("\'", n2 + 1);
		size_t n4 = strDesc.find("\'", n3 + 1);
		if ( n1 != string::npos && n2 != string::npos && n3 != string::npos && n4 != string::npos && 
			(n1 < n2) && (n2 < n3) && (n3 < n4))
		{
			string strTm11 = strDesc;
			string strTm12 = strDesc;

			// Extract name of property type
			strTm11.erase(0, n1 + 1);
			strTm11.erase(n2 - n1 - 1, string::npos);

			// Extract name of property tag
			strTm12.erase(0, n3 + 1);
			strTm12.erase(n4 - n3 - 1, string::npos);

			// build the string to search in XML:
			strTm11 += ">";
			strTm11 += strTm12;

			vector<uint32_t> lineNums;
			if (GetLineNumbers(fileName, strTm11.c_str(), lineNums) && lineNums.size() > 0)
			{
				// We actually need only first line number... me be in future we will use all found
				strLineN = to_string(lineNums[0]);
				// Line number found!
				return strLineN;
			}

		}
	}
	

	// Not found, "1" will be returned
	return strLineN;
}

string FindSwissKnifeErrorLineStr(const char *fileName, const char *strError)
{
	string strLineN("1"); // set to "1" by default

	// Reference string from source\GenApi\src\GenApi\NodeMap.cpp :
	// "Error while parsing equation for node '" << (*ptr)->GetName().c_str() << "': " << e.what();
	string strRef = "Error while parsing equation for node ";

	// Error text
	string strDesc(strError);
	
	size_t idx = strDesc.find(strRef.c_str());
	if (string::npos != idx)
	{
		// Parse the string to find the node name in quotes
		size_t n1 = strDesc.find("\'");
		size_t n2 = strDesc.find("\'", n1 + 1);
		if (n1 != string::npos && n2 != string::npos && n1 < n2)
		{
			// Extract the string in quotes - 'nodeName' 
			strDesc.erase(0, n1 + 1);
			strDesc.erase(n2 - n1 - 1, string::npos);

			vector<uint32_t> lineNums;
			if (GetLineNumbers(fileName, strDesc.c_str(), lineNums) && lineNums.size() > 0)
			{
				// We actually need only first line number... me be in future we will use all found
				strLineN = to_string(lineNums[0]);
				// Line number found!
				return strLineN;
			}

		}
	}

	return strLineN;
};

bool GetLineNumbers(const char *fileName, const char *strFind, vector<uint32_t> &lines)
{
	// 
	ifstream infl(fileName);
	string strLine;

	bool found = false;
	int i = 0;
	bool comment = false;
	while ( getline(infl, strLine) )
	{
		// Skip/erase XML commens in line
		auto rem1 = strLine.find("<!--");
		auto rem2 = strLine.find("-->");
		while (rem1 != string::npos || rem2 != string::npos)
		{
			if (rem1 != string::npos) // any rem2
			{
				strLine.erase(rem1, (rem2 == string::npos) ? rem2: rem2+3);
				comment = (rem2 == string::npos); // Set comment flag if no comment closing
			}
			else  //   rem1 == string::npos, rem2 != string::npos
			{
				strLine.erase(0, rem2+3);
				comment = false; // Reset omment flag, since we got comment closed
			}
			rem1 = strLine.find("<!--");
			rem2 = strLine.find("-->");
		}

		if ((!comment) && (string::npos != strLine.find(strFind)))
		{
			lines.push_back(i+1);
			found = true;
		}

		++i;
	}

	return found;
};