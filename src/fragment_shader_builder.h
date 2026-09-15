#pragma once
#include <cstdio>
#include <string>

bool BuildFragmentShader(std::string& out, FILE* log, bool& consumptionAvailable, bool enableConsumption=true);
