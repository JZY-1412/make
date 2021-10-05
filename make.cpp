#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>
#include <string>
#include <sys/stat.h>
#include <thread>
#include <unistd.h>
#include <vector>


using namespace std;

class Target {
public:
    string self;
    string command;
    vector<string> dependencies;
    Target() {
        this->self = "";
        this->command = "";
    }
};


string ltrim(const string& s) {
    return regex_replace(s, regex("^\\s+"), string(""));
}


string rtrim(const string& s) {
    return regex_replace(s, regex("\\s+$"), string(""));
}


pair<bool, vector<Target*>> read_makefile(const string& filename) {
    vector<Target*> targets;
    bool makefileError = false;

    string lineReader;
    string content;
    ifstream makefile(filename);

    if (!makefile.good()) {
        makefileError = true;
        cout << "Error: error with opening the makefile." << endl;
        vector<Target*> error;
        pair<bool, vector<Target*>> result;
        result.first = makefileError;
        result.second = error;
        return result;
    }

    while (getline (makefile, lineReader)) {
        if (lineReader[0] == '\t') {
            makefileError = true;
            break;
        }

        string::size_type index;
        index = lineReader.find(':');

        if (index == string::npos) continue;

        // setup self:dependencies
        auto* target = new Target();
        target->self = rtrim(ltrim(lineReader.substr(0, index)));
        stringstream ss(lineReader.substr(index + 1, lineReader.size()));
        while (ss.good()) {
            string dependency;
            getline(ss, dependency, ' ');
            if (dependency.empty()) continue;
            target->dependencies.push_back(rtrim(ltrim(dependency)));
        }
        getline(makefile, lineReader);
        if (lineReader[0] != '\t') {
            cout << lineReader << endl;
            makefileError = true;
            break;
        }
        target->command = rtrim(ltrim(lineReader.substr(1,lineReader.size())));

        targets.push_back(target);
    }
    if (makefileError) {
        cout << "Error: makefile has syntax error." << endl;
        vector<Target*> error;
        pair<bool, vector<Target*>> result;
        result.first = makefileError;
        result.second = error;
        return result;
    }
    pair<bool, vector<Target*>> result;
    result.first = makefileError;
    result.second = targets;
    return result;
}


vector<pair<int, vector<Target*>>> build_dependency_order(const vector<Target*>& targets) {
    vector<pair<int, vector<Target*>>> orderTargets;
    for (Target* target : targets) {

        if (orderTargets.empty()) {
            vector<Target*> rootT;
            rootT.push_back(target);
            pair<int, vector<Target*>> root;
            root.first = 0;
            root.second = rootT;
            orderTargets.push_back(root);
        } else {
            int parentOfOrder = 0;
            int order = 0;
            for (const pair<int, vector<Target*>>& currentLevel : orderTargets) {
                vector<Target*> otTargets = currentLevel.second;

                // check if self in otTargets' dependencies
                for (Target* otTarget : otTargets) {
                    bool child = false;
                    bool parent = false;
                    bool same = true;

                    for (const string& d : otTarget->dependencies) {
                        if (d == target->self) {
                            child = true;
                            same = false;
                        }
                    }
                    if (!child) {
                        for (const string& d : target->dependencies) {
                            if (d == otTarget->self) {
                                parent = true;
                                same = false;
                            }
                        }
                    }
                    if (child) {
                        if (order < currentLevel.first + 1) {
                            order = currentLevel.first + 1;
                        }
                    } else if (parent) {
                        order = currentLevel.first;
                        parentOfOrder = currentLevel.first;
                    } else if (same) {
                        order = currentLevel.first;
                    }
                }
            }

            bool added = false;
            bool changed = false;
            int index = 0;
            for (pair<int, vector<Target*>>& currentLevel : orderTargets) {
                if (changed) {
                    currentLevel.first = currentLevel.first + 1;
                }
                if (currentLevel.first == order) {
                    if (currentLevel.first != parentOfOrder) {
                        vector<Target*> temp = currentLevel.second;
                        temp.push_back(target);
                        currentLevel.second = temp;
                        added = true;
                        break;
                    } else {
                        currentLevel.first = currentLevel.first + 1;
                        pair<int, vector<Target*>> newLevel;
                        newLevel.first = order;
                        vector<Target*> newLevelT;
                        newLevelT.push_back(target);
                        newLevel.second =newLevelT;
                        auto it = orderTargets.begin() + index;
                        orderTargets.insert(it, newLevel);
                        added = true;
                        changed = true;
                    }
                }
                index += 1;
            }
            if (!added) {
                pair<int, vector<Target*>> newLevel;
                newLevel.first = order;
                vector<Target*> newLevelT;
                newLevelT.push_back(target);
                newLevel.second = newLevelT;
                orderTargets.push_back(newLevel);
            }
        }
    }
    return orderTargets;
}


bool file_exists (const std::string& filename) {
    struct stat buffer{};
    return (stat (filename.c_str(), &buffer) == 0);
}


bool target_in(Target* target, const vector<Target*>& level) {
    for (Target* addedTarget : level) {
        if (target->self == addedTarget->self) {
            return true;
        }
    }
    return false;
}


vector<vector<Target*>> get_need_renew_targets(const vector<pair<int, vector<Target*>>>& orderTargets) {
    vector<vector<Target*>> targetNeedRenew;
    for (const pair<int, vector<Target*>>& orderTarget : orderTargets) {
        vector<Target*> level;
        for (Target* target : orderTarget.second) {
            if (file_exists(target->self)) {
                for (const string& dependency : target->dependencies) {
                    if (!file_exists(dependency)) {
                        if (!target_in(target, level)) level.push_back(target);
                        break;
                    } else if (filesystem::last_write_time(dependency) > filesystem::last_write_time(target->self)) {
                        if (!target_in(target, level)) level.push_back(target);
                        break;
                    }
                    for (const vector<Target*>& addedLevel : targetNeedRenew) {
                        for (Target* renewTarget : addedLevel) {
                            if (dependency == renewTarget->self) {
                                if (!target_in(target, level)) level.push_back(target);
                                break;
                            }
                        }
                    }
                }
            } else {
                if (!target_in(target, level)) level.push_back(target);
            }
        }
        if (!level.empty()) targetNeedRenew.push_back(level);
    }
    return targetNeedRenew;
}


void run(const string& command) {
    system(command.c_str());
}


int main(int argc, char **argv) {
    // get argument makefile name
    int c;
    int file_flag = 0;
    string filename;
    filename =  "Makefile"; // default makefile name
    while ((c = getopt(argc, argv, "f:")) != -1) { // get argument makefile name
        if (c == 'f') {
            file_flag = 1;
            filename = optarg;
        }
    }
    if (file_flag == 0) {
        cout << "Usage: %s -f [File path]" << endl;
        cout << "No file argument provided." << endl;
        cout << "Initializing the file name with default value: " << filename << endl;
    } else {
        cout << "Using makefile: " << filename << endl;
    }

    // read makefile
    vector<Target*> targets; // record all the targets
    pair<bool, vector<Target*>> reader = read_makefile(filename);
    if (reader.first) {
        cout << endl;
        return -1;
    } else {
        targets = reader.second;
    }

    // build order
    vector<pair<int, vector<Target*>>> orderTargets = build_dependency_order(targets);
    reverse(orderTargets.begin(), orderTargets.end()); // from the lowest level to the highest level

    // check which target need to renew
    vector<vector<Target*>> targetNeedRenew = get_need_renew_targets(orderTargets);
    if (targetNeedRenew.empty()) {
        cout << "All files are up to date." << endl;
        cout << endl;
        return 0;
    }

    // run commands parallel
    for (const vector<Target*>& level : targetNeedRenew) {
        auto* workers = new thread[level.size()];
        int i = 0;
        for (Target* target : level) {
            workers[i] = thread(run, target->command);
            i++;
        }
        for (int j = 0; j < i; j++) {
            workers[j].join();
        }
    }

    cout << "Done." << endl;
    cout << endl;

    return 0;
}
