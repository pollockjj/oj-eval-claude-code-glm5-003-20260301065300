#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <algorithm>
#include <sstream>

using namespace std;

enum class Status {
    Accepted,
    Wrong_Answer,
    Runtime_Error,
    Time_Limit_Exceed
};

Status parseStatus(const string& s) {
    if (s == "Accepted") return Status::Accepted;
    if (s == "Wrong_Answer") return Status::Wrong_Answer;
    if (s == "Runtime_Error") return Status::Runtime_Error;
    return Status::Time_Limit_Exceed;
}

string statusToString(Status s) {
    switch (s) {
        case Status::Accepted: return "Accepted";
        case Status::Wrong_Answer: return "Wrong_Answer";
        case Status::Runtime_Error: return "Runtime_Error";
        case Status::Time_Limit_Exceed: return "Time_Limit_Exceed";
    }
    return "";
}

struct Submission {
    char problem;
    Status status;
    int time;
};

struct ProblemState {
    int wrongBeforeFreeze = 0;
    int submitAfterFreeze = 0;
    int wrongTotal = 0;
    bool solved = false;
    int solveTime = 0;
    bool solvedBeforeFreeze = false;
    int wrongBeforeSolve = 0;
};

struct Team {
    string name;
    map<char, ProblemState> problems;
    vector<Submission> submissions;

    // Cached values for comparison
    mutable int cachedSolved = -1;
    mutable int cachedPenalty = -1;
    mutable vector<int> cachedSolveTimes;
    mutable bool cacheValid = false;

    void invalidateCache() { cacheValid = false; }

    void updateCache(int problemCount, bool excludeFrozen) const {
        if (cacheValid) return;

        cachedSolved = 0;
        cachedPenalty = 0;
        cachedSolveTimes.clear();

        for (int i = 0; i < problemCount; i++) {
            char p = 'A' + i;
            auto it = problems.find(p);
            if (it != problems.end() && it->second.solved) {
                bool isFrozen = !it->second.solvedBeforeFreeze && it->second.submitAfterFreeze > 0;
                if (!excludeFrozen || !isFrozen) {
                    cachedSolved++;
                    cachedPenalty += 20 * it->second.wrongBeforeSolve + it->second.solveTime;
                    cachedSolveTimes.push_back(it->second.solveTime);
                }
            }
        }
        sort(cachedSolveTimes.begin(), cachedSolveTimes.end(), greater<int>());
        cacheValid = true;
    }
};

class ICPCSystem {
private:
    unordered_map<string, Team> teams;
    bool started = false;
    int duration = 0;
    int problemCount = 0;
    bool frozen = false;
    bool flushed = false;
    vector<string> teamOrder;

    void invalidateAllCaches() {
        for (auto& p : teams) {
            p.second.invalidateCache();
        }
    }

    bool compareTeams(const string& a, const string& b, bool excludeFrozen) const {
        auto itA = teams.find(a);
        auto itB = teams.find(b);
        const Team& teamA = itA->second;
        const Team& teamB = itB->second;

        teamA.updateCache(problemCount, excludeFrozen);
        teamB.updateCache(problemCount, excludeFrozen);

        if (teamA.cachedSolved != teamB.cachedSolved)
            return teamA.cachedSolved > teamB.cachedSolved;
        if (teamA.cachedPenalty != teamB.cachedPenalty)
            return teamA.cachedPenalty < teamB.cachedPenalty;

        for (size_t i = 0; i < min(teamA.cachedSolveTimes.size(), teamB.cachedSolveTimes.size()); i++) {
            if (teamA.cachedSolveTimes[i] != teamB.cachedSolveTimes[i])
                return teamA.cachedSolveTimes[i] < teamB.cachedSolveTimes[i];
        }

        return a < b;
    }

    vector<string> getRankingOrder(bool excludeFrozen) const {
        vector<string> order;
        order.reserve(teams.size());
        for (const auto& p : teams) {
            order.push_back(p.first);
        }
        sort(order.begin(), order.end(), [this, excludeFrozen](const string& a, const string& b) {
            return this->compareTeams(a, b, excludeFrozen);
        });
        return order;
    }

    void flushScoreboard() {
        teamOrder = getRankingOrder(true);
        flushed = true;
    }

    string formatProblemStatus(const Team& team, char p, bool showFrozen) const {
        auto it = team.problems.find(p);
        if (it == team.problems.end()) {
            return ".";
        }

        const ProblemState& ps = it->second;
        bool isFrozen = showFrozen && !ps.solvedBeforeFreeze && ps.submitAfterFreeze > 0;

        if (isFrozen) {
            int wrong = ps.wrongBeforeFreeze;
            if (wrong == 0) {
                return "0/" + to_string(ps.submitAfterFreeze);
            } else {
                return "-" + to_string(wrong) + "/" + to_string(ps.submitAfterFreeze);
            }
        } else if (ps.solved) {
            int wrong = ps.wrongBeforeSolve;
            if (wrong == 0) {
                return "+";
            } else {
                return "+" + to_string(wrong);
            }
        } else {
            int wrong = ps.wrongTotal;
            if (wrong == 0) {
                return ".";
            } else {
                return "-" + to_string(wrong);
            }
        }
    }

    void printTeam(const string& name, int rank, bool excludeFrozen, bool showFrozen) const {
        const Team& team = teams.at(name);
        team.updateCache(problemCount, excludeFrozen);

        cout << name << " " << rank << " " << team.cachedSolved << " " << team.cachedPenalty;
        for (int i = 0; i < problemCount; i++) {
            char p = 'A' + i;
            cout << " " << formatProblemStatus(team, p, showFrozen);
        }
        cout << "\n";
    }

    void printScoreboard(bool excludeFrozen, bool showFrozen) const {
        vector<string> order = getRankingOrder(excludeFrozen);

        int rank = 1;
        for (size_t i = 0; i < order.size(); i++) {
            if (i > 0 && compareTeams(order[i-1], order[i], excludeFrozen)) {
                rank = i + 1;
            }
            printTeam(order[i], rank, excludeFrozen, showFrozen);
        }
    }

    int getTeamRankFromOrder(const string& name, const vector<string>& order, bool excludeFrozen) const {
        for (size_t i = 0; i < order.size(); i++) {
            if (order[i] == name) {
                int rank = 1;
                for (size_t j = 0; j < i; j++) {
                    if (compareTeams(order[j], order[i], excludeFrozen)) {
                        rank = j + 2;
                    }
                }
                return rank;
            }
        }
        return -1;
    }

    int getStoredRank(const string& name) const {
        if (!flushed) {
            vector<string> order;
            order.reserve(teams.size());
            for (const auto& p : teams) {
                order.push_back(p.first);
            }
            sort(order.begin(), order.end());
            for (size_t i = 0; i < order.size(); i++) {
                if (order[i] == name) return i + 1;
            }
            return -1;
        }
        return getTeamRankFromOrder(name, teamOrder, true);
    }

    bool hasFrozenProblems(const Team& team) const {
        for (int i = 0; i < problemCount; i++) {
            char p = 'A' + i;
            auto it = team.problems.find(p);
            if (it != team.problems.end()) {
                if (!it->second.solvedBeforeFreeze && it->second.submitAfterFreeze > 0) {
                    return true;
                }
            }
        }
        return false;
    }

    char getSmallestFrozenProblem(const Team& team) const {
        for (int i = 0; i < problemCount; i++) {
            char p = 'A' + i;
            auto it = team.problems.find(p);
            if (it != team.problems.end()) {
                if (!it->second.solvedBeforeFreeze && it->second.submitAfterFreeze > 0) {
                    return p;
                }
            }
        }
        return '\0';
    }

    void unfreezeProblem(Team& team, char p) {
        auto it = team.problems.find(p);
        if (it != team.problems.end()) {
            ProblemState& ps = it->second;

            int totalSubs = 0;
            for (const auto& sub : team.submissions) {
                if (sub.problem == p) {
                    totalSubs++;
                }
            }

            int preFreezeCount = totalSubs - ps.submitAfterFreeze;
            int idx = 0;
            int acceptedTime = -1;
            bool solvedDuringFreeze = false;
            int wrongDuringFreeze = 0;

            for (const auto& sub : team.submissions) {
                if (sub.problem == p) {
                    idx++;
                    if (idx > preFreezeCount) {
                        if (sub.status == Status::Accepted && acceptedTime == -1) {
                            acceptedTime = sub.time;
                            solvedDuringFreeze = true;
                        } else if (acceptedTime == -1) {
                            wrongDuringFreeze++;
                        }
                    }
                }
            }

            if (solvedDuringFreeze) {
                ps.solved = true;
                ps.solveTime = acceptedTime;
                ps.wrongBeforeSolve = ps.wrongBeforeFreeze + wrongDuringFreeze;
                ps.wrongTotal = ps.wrongBeforeSolve;
            } else {
                ps.wrongTotal = ps.wrongBeforeFreeze + wrongDuringFreeze;
            }

            ps.submitAfterFreeze = 0;
            team.invalidateCache();
        }
    }

    void scroll() {
        if (!frozen) {
            cout << "[Error]Scroll failed: scoreboard has not been frozen.\n";
            return;
        }

        cout << "[Info]Scroll scoreboard.\n";
        printScoreboard(true, true);

        vector<tuple<string, string, int, int>> changes;
        invalidateAllCaches();
        vector<string> prevOrder = getRankingOrder(true);

        while (true) {
            vector<string> teamsWithFrozen;
            teamsWithFrozen.reserve(teams.size());
            for (const auto& p : teams) {
                if (hasFrozenProblems(p.second)) {
                    teamsWithFrozen.push_back(p.first);
                }
            }

            if (teamsWithFrozen.empty()) break;

            sort(teamsWithFrozen.begin(), teamsWithFrozen.end(), [this](const string& a, const string& b) {
                return this->compareTeams(a, b, true);  // Use excludeFrozen for current scoreboard ranking
            });

            string teamName = teamsWithFrozen.back();
            char problem = getSmallestFrozenProblem(teams[teamName]);

            if (problem == '\0') break;

            int oldRank = getTeamRankFromOrder(teamName, prevOrder, true);

            unfreezeProblem(teams[teamName], problem);

            invalidateAllCaches();
            vector<string> newOrder = getRankingOrder(false);

            int newRank = getTeamRankFromOrder(teamName, newOrder, false);

            if (newRank < oldRank) {
                string replacedTeam = prevOrder[newRank - 1];
                teams[teamName].updateCache(problemCount, false);
                int newSolved = teams[teamName].cachedSolved;
                int newPenalty = teams[teamName].cachedPenalty;
                changes.push_back({teamName, replacedTeam, newSolved, newPenalty});
            }

            prevOrder = newOrder;
        }

        for (const auto& c : changes) {
            cout << get<0>(c) << " " << get<1>(c) << " " << get<2>(c) << " " << get<3>(c) << "\n";
        }

        invalidateAllCaches();
        printScoreboard(false, false);

        frozen = false;
        flushScoreboard();
    }

public:
    void addTeam(const string& name) {
        if (started) {
            cout << "[Error]Add failed: competition has started.\n";
            return;
        }
        if (teams.count(name)) {
            cout << "[Error]Add failed: duplicated team name.\n";
            return;
        }
        teams[name] = Team{name};
        cout << "[Info]Add successfully.\n";
    }

    void startCompetition(int dur, int probs) {
        if (started) {
            cout << "[Error]Start failed: competition has started.\n";
            return;
        }
        duration = dur;
        problemCount = probs;
        started = true;
        cout << "[Info]Competition starts.\n";
    }

    void submit(const string& problem, const string& teamName, const string& status, int time) {
        if (!started) return;

        Status s = parseStatus(status);
        auto& team = teams[teamName];
        char p = problem[0];

        team.submissions.push_back({p, s, time});

        auto& ps = team.problems[p];

        if (frozen) {
            if (!ps.solvedBeforeFreeze) {
                ps.submitAfterFreeze++;
            }
        } else {
            if (s == Status::Accepted && !ps.solved) {
                ps.solved = true;
                ps.solveTime = time;
                ps.wrongBeforeSolve = ps.wrongTotal;
                ps.solvedBeforeFreeze = true;
            } else if (s != Status::Accepted && !ps.solved) {
                ps.wrongTotal++;
            }
        }
        team.invalidateCache();
    }

    void flush() {
        invalidateAllCaches();
        flushScoreboard();
        cout << "[Info]Flush scoreboard.\n";
    }

    void freeze() {
        if (frozen) {
            cout << "[Error]Freeze failed: scoreboard has been frozen.\n";
            return;
        }
        for (auto& p : teams) {
            for (auto& q : p.second.problems) {
                if (q.second.solved) {
                    q.second.solvedBeforeFreeze = true;
                }
                q.second.wrongBeforeFreeze = q.second.wrongTotal;
            }
        }
        frozen = true;
        cout << "[Info]Freeze scoreboard.\n";
    }

    void performScroll() {
        scroll();
    }

    void queryRanking(const string& teamName) {
        if (!teams.count(teamName)) {
            cout << "[Error]Query ranking failed: cannot find the team.\n";
            return;
        }

        cout << "[Info]Complete query ranking.\n";

        if (frozen) {
            cout << "[Warning]Scoreboard is frozen. The ranking may be inaccurate until it were scrolled.\n";
        }

        int rank = getStoredRank(teamName);
        cout << teamName << " NOW AT RANKING " << rank << "\n";
    }

    void querySubmission(const string& teamName, const string& problem, const string& status) {
        if (!teams.count(teamName)) {
            cout << "[Error]Query submission failed: cannot find the team.\n";
            return;
        }

        cout << "[Info]Complete query submission.\n";

        const Team& team = teams.at(teamName);
        const Submission* lastMatch = nullptr;

        for (const auto& sub : team.submissions) {
            bool match = true;

            if (problem != "ALL") {
                if (string(1, sub.problem) != problem) match = false;
            }

            if (status != "ALL") {
                if (statusToString(sub.status) != status) match = false;
            }

            if (match) {
                lastMatch = &sub;
            }
        }

        if (!lastMatch) {
            cout << "Cannot find any submission.\n";
        } else {
            cout << teamName << " " << lastMatch->problem << " "
                 << statusToString(lastMatch->status) << " " << lastMatch->time << "\n";
        }
    }

    void end() {
        cout << "[Info]Competition ends.\n";
    }
};

vector<string> split(const string& s) {
    vector<string> tokens;
    string token;
    istringstream iss(s);
    while (iss >> token) {
        tokens.push_back(token);
    }
    return tokens;
}

int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    ICPCSystem system;
    string line;

    while (getline(cin, line)) {
        if (line.empty()) continue;

        vector<string> tokens = split(line);
        string cmd = tokens[0];

        if (cmd == "ADDTEAM") {
            system.addTeam(tokens[1]);
        } else if (cmd == "START") {
            int dur = stoi(tokens[2]);
            int probs = stoi(tokens[4]);
            system.startCompetition(dur, probs);
        } else if (cmd == "SUBMIT") {
            string problem = tokens[1];
            string teamName = tokens[3];
            string status = tokens[5];
            int time = stoi(tokens[7]);
            system.submit(problem, teamName, status, time);
        } else if (cmd == "FLUSH") {
            system.flush();
        } else if (cmd == "FREEZE") {
            system.freeze();
        } else if (cmd == "SCROLL") {
            system.performScroll();
        } else if (cmd == "QUERY_RANKING") {
            system.queryRanking(tokens[1]);
        } else if (cmd == "QUERY_SUBMISSION") {
            string teamName = tokens[1];
            string problem = tokens[3].substr(8);
            string status = tokens[5].substr(7);
            system.querySubmission(teamName, problem, status);
        } else if (cmd == "END") {
            system.end();
            break;
        }
    }

    return 0;
}
