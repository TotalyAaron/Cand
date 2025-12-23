#ifndef __CACCFG_H__
#define __CACCFG_H__
#define __CAND_VERSION__ 1.000
#include <string>
bool WError = false;
char W = 3;
std::string errMsg = "";
std::vector<std::string> allBIDataTypes = {"int1", "bool", "int8", "int16", "int32", "ing64", "in128", "float32", "float64", "char"};
#endif