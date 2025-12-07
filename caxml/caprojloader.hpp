#ifndef __CAPROJLOADER_HPP__
#define __CAPROJLOADER_HPP__
#include "../tinyxml2.h"
#include <string>
#include <iostream>
#include <filesystem>
#include "llvm/Passes/OptimizationLevel.h"
using namespace tinyxml2;
llvm::OptimizationLevel checkOptimizationLevel(std::string oplevel)
{
    if (oplevel == "O0")
        return llvm::OptimizationLevel::O0;
    else if (oplevel == "O1")
        return llvm::OptimizationLevel::O1;
    else if (oplevel == "02")
        return llvm::OptimizationLevel::O2;
    else if (oplevel == "O3")
        return llvm::OptimizationLevel::O3;
    else if (oplevel == "Os")
        return llvm::OptimizationLevel::Os;
    else if (oplevel == "Oz")
        return llvm::OptimizationLevel::Oz;
    else
        return llvm::OptimizationLevel::O3;
}
struct Program {
    bool success = false;
    std::string ModuleID = "main";
    std::string SourceFilename = "main.ca";
    std::string outputPath = std::filesystem::current_path().string() + "/main.a";
    std::string mode = "-s";
    llvm::OptimizationLevel optimizationLevel = llvm::OptimizationLevel::O1;
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
Project* LoadProject(const std::string& filename) {
    XMLDocument doc;
    if (doc.LoadFile(filename.c_str()) != XML_SUCCESS)
        return new Project();
    XMLElement* root = doc.FirstChildElement("Project");
    if (!root) return new Project();
    // Read Name element
    XMLElement* nameElem = root->FirstChildElement("Name");
    std::string projectName = nameElem && nameElem->GetText()
        ? nameElem->GetText()
        : "default";
    // Program
    XMLElement* fileElement = root->FirstChildElement("Program");
    if (!fileElement) return new Project();
    // Attributes
    std::string moduleID =
        fileElement->FirstChildElement("ModuleID") &&
        fileElement->FirstChildElement("ModuleID")->GetText()
            ? fileElement->FirstChildElement("ModuleID")->GetText()
            : projectName;

    std::string srcfn =
        fileElement->FirstChildElement("SourceFilename") &&
        fileElement->FirstChildElement("SourceFilename")->GetText()
            ? fileElement->FirstChildElement("SourceFilename")->GetText()
            : (projectName + ".ca");

    std::string op =
        fileElement->FirstChildElement("OutputPath") &&
        fileElement->FirstChildElement("OutputPath")->GetText()
            ? fileElement->FirstChildElement("OutputPath")->GetText()
            : (std::filesystem::current_path().string() + "/" + projectName + ".a");

    std::string mode =
        fileElement->FirstChildElement("OutputMode") &&
        fileElement->FirstChildElement("OutputMode")->GetText()
            ? fileElement->FirstChildElement("OutputMode")->GetText()
            : "";

    std::string optlvl =
        fileElement->FirstChildElement("OptimizationLevel") &&
        fileElement->FirstChildElement("OptimizationLevel")->GetText()
            ? fileElement->FirstChildElement("OptimizationLevel")->GetText()
            : "O1";

    Project* proj = new Project(Program(moduleID, srcfn, op));
    proj->program.mode = mode;
    proj->program.optimizationLevel = checkOptimizationLevel(optlvl);
    return proj;
}
Project *currentProject = nullptr;
#endif