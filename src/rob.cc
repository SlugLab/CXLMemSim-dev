#include "rob.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <ranges>
#include <cxxopts.hpp>
#include <spdlog/cfg/env.h>

Rob::Rob() {
    controller = nullptr;
}


Rob::~Rob() {
    controller = nullptr;
}

void Rob::issue(InstructionGroup ins){

}

void Rob::add_request(uint64_t addr, bool is_write) {
    lru.push_back(addr);
}

void Rob::evict_lru() {
    uint64_t addr = lru.front();
    lru.pop_front();
    addr_to_lru_idx.erase(addr);
    lru_idx_to_addr.erase(0);
}
Helper helper{};
CXLController *controller;
Monitors *monitors;

// Simple trim function to remove leading/trailing whitespace.
std::string trim(const std::string &s) {
    auto start = s.begin();
    while (start != s.end() && std::isspace(*start))
        start++;
    auto end = s.end();
    do {
        end--;
    } while (std::distance(start, end) > 0 && std::isspace(*end));
    return std::string(start, end + 1);
}



// Parse the full group (a vector of strings) to extract the fetch header info and
// the following instruction lines. If an "O3PipeView:address:" line is present later,
// it will override the address.
InstructionGroup parseGroup(const std::vector<std::string>& group) {
    InstructionGroup ig;
    int fetchIndex = -1;
    
    // Find the fetch header (first line that starts with "O3PipeView:fetch:")
    for (size_t i = 0; i < group.size(); i++) {
        if (group[i].rfind("O3PipeView:fetch:", 0) == 0) {
            fetchIndex = static_cast<int>(i);
            break;
        }
    }
    if (fetchIndex == -1)
        return ig; // No fetch header found.
    
    // Parse the fetch header line.
    {
        std::istringstream iss(group[fetchIndex]);
        std::string token;
        std::vector<std::string> tokens;
        while (std::getline(iss, token, ':')) {
            tokens.push_back(token);
        }
        // Expected token layout:
        // tokens[0] = "O3PipeView"
        // tokens[1] = "fetch"
        // tokens[2] = timestamp (e.g., "816000")
        // tokens[3] = address (e.g., "0x7ffff801f7f7")
        // tokens[4] = extra field (often "0")
        // tokens[5] = cycle count (e.g., "1686")
        // There may be an extra empty token if the header line ends with a colon.
        if (tokens.size() >= 6) {
            // ig.address = tokens[3];
            try {
                ig.fetchTimestamp = std::stoull(tokens[2]);
                ig.cycleCount = std::stoull(tokens[5]);
            } catch (...) {
                ig.fetchTimestamp = 0;
                ig.cycleCount = 0;
            }
        }
    }
    
    // Combine the instruction lines.
    // We assume that the instruction details follow the fetch header until a line starting
    // with "O3PipeView:" is encountered.
    std::string inst;
    for (size_t i = fetchIndex + 1; i < group.size(); i++) {
        // If the line starts with "O3PipeView:" it is a stage indicator.
        // (We still want to check later for an "O3PipeView:address:" line.)
        if (group[i].rfind("O3PipeView:", 0) == 0)
            continue;
        if (!inst.empty())
            inst += " ";
        inst += trim(group[i]);
    }
    ig.instruction = inst;
    
    ig.address = 0;
    // Look for an override address in any line starting with "O3PipeView:address:".
    for (const auto &line : group) {
        if (line.rfind("O3PipeView:address:", 0) == 0) {
            std::istringstream iss(line);
            std::string token;
            std::vector<std::string> tokens;
            while (std::getline(iss, token, ':')) {
                tokens.push_back(token);
            }
            // Expected tokens:
            // tokens[0] = "O3PipeView"
            // tokens[1] = "address"
            // tokens[2] = the address (e.g., "140737354362576")
            if (tokens.size() >= 3) {
                ig.address = std::stoull(tokens[2]);
            }
        }
    }
        for (const auto &line : group) {
        if (line.rfind("O3PipeView:retire:", 0) == 0) {
            std::istringstream iss(line);
            std::string token;
            std::vector<std::string> tokens;
            while (std::getline(iss, token, ':')) {
                tokens.push_back(token);
            }
            // Expected tokens:
            // tokens[0] = "O3PipeView"
            // tokens[1] = "retire"
            // tokens[2] = the timestamp (e.g., "820000")
            // tokens[3] = the store or load (e.g., "store" or "load")
            // tokens[4] = the timestamp (e.g., "0x7ffff801f7f7")
            if (tokens.size() >= 5) {
                ig.retireTimestamp = std::stoull(tokens[2]);
            }
        }
    }
    return ig;
}
int main(int argc, char *argv[]) {
        spdlog::cfg::load_env_levels();
    cxxopts::Options options("CXLMemSim", "For simulation of CXL.mem Type 3 on Sapphire Rapids");
    options.add_options()("t,target", "The script file to execute",
                          cxxopts::value<std::string>()->default_value("/trace.out"))(
        "h,help", "Help for CXLMemSimRoB", cxxopts::value<bool>()->default_value("false"))(
        "o,topology", "The newick tree input for the CXL memory expander topology",
        cxxopts::value<std::string>()->default_value("(1,(2,3))"))(
        "e,capacity", "The capacity vector of the CXL memory expander with the firsgt local",
        cxxopts::value<std::vector<int>>()->default_value("0,20,20,20"))(
        "f,frequency", "The frequency for the running thread", cxxopts::value<double>()->default_value("4000"))(
        "l,latency", "The simulated latency by epoch based calculation for injected latency",
        cxxopts::value<std::vector<int>>()->default_value("100,150,100,150,100,150"))(
        "b,bandwidth", "The simulated bandwidth by linear regression",
        cxxopts::value<std::vector<int>>()->default_value("50,50,50,50,50,50"));

    auto result = options.parse(argc, argv);
    if (result["help"].as<bool>()) {
        std::cout << options.help() << std::endl;
        exit(0);
    }
    auto target = result["target"].as<std::string>();
    Rob rob;

    // read from file
    std::ifstream file(target);
    if (!file) {
        std::cerr << "Failed to open /trace.txt\n";
        return 1;
    }

    std::vector<std::string> groupLines;
    std::vector<InstructionGroup> instructions;
    
    // Read the file line by line using std::ranges::istream_view.
    for (const std::string &line : std::ranges::istream_view<std::string>(file)) {
        // If the line starts with "O3PipeView:fetch:" then it's the start of a new group.
        if (line.rfind("O3PipeView:fetch:", 0) == 0) {
            // If we have an existing group, process it.
            if (!groupLines.empty()) {
                instructions.emplace_back(parseGroup(groupLines));
		if(instructions.back().retireTimestamp == 0) {
			//auto& back = instructions.back();
			//std::cout << "throwing out: " << back.address << back.cycleCount << "[]" << back.retireTimestamp << std::endl;
			instructions.pop_back();}
                // Clear the group for the next one.
                groupLines.clear();
            }
        }
        // Add the current line to the group if it’s not empty.
        if (!line.empty()) {
            groupLines.push_back(line);
        }
    }
    // Process any remaining group.
    if (!groupLines.empty()) {
        instructions.emplace_back(parseGroup(groupLines));
    }
    
    std::sort(instructions.begin(), instructions.end(), [](InstructionGroup& a, InstructionGroup& b)
                                  {
                                      return a.cycleCount < b.cycleCount;
                                  });
    for( const auto& instruction : instructions){
	    std::cout << instruction.cycleCount << ":" << instruction.address <<std::endl;
    }
    // After processing all groups, call your ROB method.
    rob.evict_lru();
    return 0;
}

