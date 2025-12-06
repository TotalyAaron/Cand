#ifndef __CAPROJLOADER_HPP__
#define __CAPROJLOADER_HPP__
#include "../tinyxml2.h"
#include <string>
#include <iostream>
using namespace tinyxml2;
struct Program {
    bool success = false;
    std::string ModuleID;
    std::string SourceFilename;
    std::string outputPath;
    Program(const std::string& modid, const std::string& sfn, const std::string& op) : ModuleID(modid), SourceFilename(sfn), outputPath(op) {
        if (!(modid == "" && sfn == "" && op == "")) success = true;
    }
    Program() = default;
};
struct Project final {
    Program program;
    Project(Program prog) : program(prog) {}
    Project() = default;
};
Project* LoadProject(std::string filename) {
    XMLDocument doc;
    if (doc.LoadFile(filename.c_str()) != XML_SUCCESS) {
        std::cout << "Failed to load file\n";
        return new Project(Program("", "", ""));
    }
    XMLElement* root = doc.FirstChildElement("Project");
    if (root) {
        const char* projectName = root->Attribute("Name");
        std::cout << "Project name: " << projectName << "\n";
        XMLElement* fileElement = root->FirstChildElement("Program");
        if (fileElement) {
            std::string moduleID = fileElement->Attribute("ModuleID") ? fileElement->Attribute("ModuleID") : "";
            std::string srcfn = fileElement->Attribute("SourceFilename") ? fileElement->Attribute("SourceFilename") : "";
            std::string op = fileElement->Attribute("OutputPath") ? fileElement->Attribute("OutputPath") : "";
            std::cout << "ModuleID: " << moduleID << "\n";
            std::cout << "SourceFilename: " << srcfn << "\n";
            std::cout << "Output path: " << op << "\n";
            return new Project(Program(moduleID, srcfn, op));
        }
        return new Project(Program("", "", ""));
    }
    return new Project(Program("", "", ""));
}
#endif