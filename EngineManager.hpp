#pragma once
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <mutex>
#include <iostream>
#include <sstream>
#include <map>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

#include <cwchar>

class EngineManager {
public:
    std::string name = "Engine"; 
    
    int threads = 1;
    int limit_depth = 21;
    int limit_nodes = 0;    
    // New Settings
    int hash = 16; // MB
    int multipv = 1;
    bool show_wdl = false;
    bool ponder = false;
    
    int skill_level = 20; 
    int move_overhead = 100;

    // Lc0 Specific
    std::string backend = "onnx-dml";
    std::string weights_path = "";
    int nn_cache_size = 200000;
    int minibatch_size = 256;
    int ram_limit_mb = 4096;
    int batch_size = 256; 

    struct AnalysisInfo {
        // Summary / Best Line Stats
        int score_cp = 0;      
        int score_mate = 0;    
        std::string best_move; 
        std::string pv;        
        long long nps = 0;     
        long long nodes = 0;
        int depth = 0;
        int wdl_w = 0;
        int wdl_d = 0;
        int wdl_l = 0;
        
        struct LineInfo {
            int score_cp = 0;
            int score_mate = 0;
            std::string pv;
            int wdl_w = 0;
            int wdl_d = 0;
            int wdl_l = 0;
        };
        std::map<int, LineInfo> lines;
    };

    EngineManager(std::string n = "Engine") : name(n) {}
    ~EngineManager() { StopEngine(); }
    
    std::atomic<bool> options_dirty{true};

    void ApplyOptions() {
        if (!is_running) return;
        options_dirty = true;
    }

    void ApplyOptionsInternal() {
        options_dirty = false;
        SendCommand("setoption name Threads value " + std::to_string(threads));
        SendCommand("setoption name MultiPV value " + std::to_string(multipv));
        SendCommand("setoption name Ponder value " + std::string(ponder ? "true" : "false"));
        SendCommand("setoption name UCI_ShowWDL value " + std::string(show_wdl ? "true" : "false"));

        if (name == "Stockfish") {
            SendCommand("setoption name Hash value " + std::to_string(hash));
        } else if (name == "Lc0") {
            SendCommand("setoption name Backend value " + backend);
            SendCommand("setoption name NNCacheSize value " + std::to_string(nn_cache_size));
            SendCommand("setoption name MinibatchSize value " + std::to_string(minibatch_size));
            SendCommand("setoption name RamLimitMb value " + std::to_string(ram_limit_mb));
        }
    }    
    void ClearHash() {
        if (!is_running) return;
        if (name == "Stockfish") {
            SendCommand("setoption name Clear Hash");
        }
    }

    bool StartEngine(const std::string& cmd_line, const std::string& working_dir = "") {
        if (is_running) return true;

#ifdef _WIN32
        SECURITY_ATTRIBUTES saAttr;
        saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
        saAttr.bInheritHandle = TRUE;
        saAttr.lpSecurityDescriptor = NULL;

        if (!CreatePipe(&hChildStd_OUT_Rd, &hChildStd_OUT_Wr, &saAttr, 0)) return false;
        SetHandleInformation(hChildStd_OUT_Rd, HANDLE_FLAG_INHERIT, 0);
        if (!CreatePipe(&hChildStd_IN_Rd, &hChildStd_IN_Wr, &saAttr, 0)) return false;
        SetHandleInformation(hChildStd_IN_Wr, HANDLE_FLAG_INHERIT, 0);

        auto ToWide = [](const std::string& str) -> std::wstring {
            if (str.empty()) return L"";
            int size_needed = MultiByteToWideChar(CP_ACP, 0, &str[0], (int)str.size(), NULL, 0);
            std::wstring wstr(size_needed, 0);
            MultiByteToWideChar(CP_ACP, 0, &str[0], (int)str.size(), &wstr[0], size_needed);
            return wstr;
        };

        std::wstring cmdLineW = ToWide(cmd_line);
        std::wstring workDirW = ToWide(working_dir);

        std::vector<wchar_t> envBlock;
        if (!working_dir.empty()) {
            LPWCH envStrings = GetEnvironmentStringsW();
            if (envStrings) {
                LPWCH current = envStrings;
                while (*current) {
                    std::wstring var(current);
                    if (var.length() >= 5 && (_wcsnicmp(var.c_str(), L"PATH=", 5) == 0)) {
                         std::wstring newPath = L"PATH=" + workDirW + L";" + var.substr(5);
                         for (wchar_t c : newPath) envBlock.push_back(c);
                         envBlock.push_back(L'\0');
                    } else {
                         for (wchar_t c : var) envBlock.push_back(c);
                         envBlock.push_back(L'\0');
                    }
                    current += wcslen(current) + 1;
                }
                FreeEnvironmentStringsW(envStrings);
                envBlock.push_back(L'\0');
            }
        }

        const wchar_t* wd = workDirW.empty() ? NULL : workDirW.c_str();

        STARTUPINFOW siStartInfo;
        PROCESS_INFORMATION piProcInfo;
        ZeroMemory(&piProcInfo, sizeof(PROCESS_INFORMATION));
        ZeroMemory(&siStartInfo, sizeof(STARTUPINFOW));
        siStartInfo.cb = sizeof(STARTUPINFOW);
        siStartInfo.hStdError = hChildStd_OUT_Wr; 
        siStartInfo.hStdOutput = hChildStd_OUT_Wr;
        siStartInfo.hStdInput = hChildStd_IN_Rd;
        siStartInfo.dwFlags |= STARTF_USESTDHANDLES;

        if (!CreateProcessW(NULL, &cmdLineW[0], NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, wd, &siStartInfo, &piProcInfo)) {
            return false;
        }

        hProcess = piProcInfo.hProcess;
        hThread = piProcInfo.hThread;
        CloseHandle(hChildStd_OUT_Wr);
        CloseHandle(hChildStd_IN_Rd);
#endif

        is_running = true;
        reader_thread = std::thread(&EngineManager::ReadLoop, this);
        SendCommand("uci");
        ApplyOptions(); 
        return true;
    }

    void StopEngine() {
        if (!is_running) return;
        is_running = false;
        SendCommand("quit");
        
#ifdef _WIN32
        // Give the engine up to 500ms to process 'quit' and exit gracefully
        if (hProcess) {
            if (WaitForSingleObject(hProcess, 500) != WAIT_OBJECT_0) {
                TerminateProcess(hProcess, 1); // Force kill if it hangs
            }
        }
#endif

        if (reader_thread.joinable()) reader_thread.join();
        
#ifdef _WIN32
        if (hProcess) { CloseHandle(hProcess); hProcess = NULL; }
        if (hThread) { CloseHandle(hThread); hThread = NULL; }
        if (hChildStd_OUT_Rd) { CloseHandle(hChildStd_OUT_Rd); hChildStd_OUT_Rd = NULL; }
        if (hChildStd_IN_Wr) { CloseHandle(hChildStd_IN_Wr); hChildStd_IN_Wr = NULL; }
#endif
    }

    void StartAnalysis(const std::string& fen) {
        if (!is_running) return;

        {
            std::lock_guard<std::mutex> lock(info_mutex);
            current_info = AnalysisInfo(); // Clear stale info immediately
        }

        if (is_searching) {
            std::lock_guard<std::mutex> lock(info_mutex);
            pending_fen = fen;
            has_pending_start = true;
            ignore_stale_info = true;
            SendCommand("stop"); 
            return;
        }

        InternalStartSearch(fen);
    }

    void StopAnalysis() {
        if (is_analyzing || is_searching) {
            has_pending_start = false; // Cancel any pending starts
            ignore_stale_info = true;
            SendCommand("stop");
            is_analyzing = false;
        }
    }

    void InternalStartSearch(const std::string& fen) {
        if (options_dirty) ApplyOptionsInternal();

        SendCommand("position fen " + fen);
        
        if (name == "Lc0" && limit_nodes > 0) SendCommand("go nodes " + std::to_string(limit_nodes));
        else if (limit_depth > 0) SendCommand("go depth " + std::to_string(limit_depth));
        else SendCommand("go infinite");
        
        is_analyzing = true;
        is_searching = true;
    }

    AnalysisInfo GetInfo() {
        std::lock_guard<std::mutex> lock(info_mutex);
        return current_info;
    }
    
    std::string GetLastLine() {
        std::lock_guard<std::mutex> lock(info_mutex);
        return last_line_seen;
    }
    
    bool IsRunning() const { return is_running; }
    bool IsSearching() const { return is_searching; }

private:
#ifdef _WIN32
    HANDLE hChildStd_IN_Rd = NULL;
    HANDLE hChildStd_IN_Wr = NULL;
    HANDLE hChildStd_OUT_Rd = NULL;
    HANDLE hChildStd_OUT_Wr = NULL;
    HANDLE hProcess = NULL;
    HANDLE hThread = NULL;
#endif

    std::atomic<bool> is_running{false};
    std::atomic<bool> is_analyzing{false};
    std::atomic<bool> is_searching{false};
    
    std::string pending_fen;
    std::atomic<bool> has_pending_start{false};
    std::atomic<bool> ignore_stale_info{false};
    
    std::thread reader_thread;
    
    AnalysisInfo current_info;
    std::string last_line_seen; // Debug
    std::mutex info_mutex;

    void SendCommand(const std::string& cmd) {
#ifdef _WIN32
        if (!hChildStd_IN_Wr) return;
        std::string full_cmd = cmd + "\n";
        DWORD written;
        WriteFile(hChildStd_IN_Wr, full_cmd.c_str(), (DWORD)full_cmd.size(), &written, NULL);
#endif
    }

    void ReadLoop() {
        char buffer[4096];
        std::string line_buffer;

        while (is_running) {
#ifdef _WIN32
            DWORD read;
            if (!ReadFile(hChildStd_OUT_Rd, buffer, sizeof(buffer) - 1, &read, NULL) || read == 0) break;
            buffer[read] = '\0';
            line_buffer += buffer;
#endif
            size_t pos;
            while ((pos = line_buffer.find('\n')) != std::string::npos) {
                std::string line = line_buffer.substr(0, pos);
                line_buffer.erase(0, pos + 1);
                if (!line.empty() && line.back() == '\r') line.pop_back();
                
                {
                    std::lock_guard<std::mutex> lock(info_mutex);
                    last_line_seen = line;
                }

                // Check for bestmove
                if (line.rfind("bestmove", 0) == 0) { // Starts with "bestmove"
                    std::stringstream ss(line);
                    std::string token, move;
                    ss >> token >> move;
                    {
                        std::lock_guard<std::mutex> lock(info_mutex);
                        if (!ignore_stale_info) {
                            current_info.best_move = move;
                        }
                    }
                    is_searching = false;
                    is_analyzing = false;
                    ignore_stale_info = false;
                    
                    if (has_pending_start) {
                        has_pending_start = false;
                        InternalStartSearch(pending_fen);
                    }
                }
                
                ParseLine(line);
            }
        }
    }

    void ParseLine(const std::string& line) {
        if (ignore_stale_info) return;
        if (line.substr(0, 4) != "info") return;

        // Temp vars for this line
        int l_multipv = 1;
        int l_depth = 0;
        long long l_nps = 0;
        long long l_nodes = 0;
        int l_score_cp = 0, l_score_mate = 0;
        int l_wdl_w = 0, l_wdl_d = 0, l_wdl_l = 0;
        std::string l_pv;

        std::stringstream ss(line);
        std::string token;
        bool reading_pv = false;

        while (ss >> token) {
            if (reading_pv) {
                if (token == "multipv" || token == "string") { reading_pv = false; break; }
                else {
                    if (!l_pv.empty()) l_pv += " ";
                    l_pv += token;
                }
                continue;
            }

            if (token == "depth") ss >> l_depth;
            else if (token == "multipv") ss >> l_multipv;
            else if (token == "nps") ss >> l_nps;
            else if (token == "nodes") ss >> l_nodes;
            else if (token == "score") {
                std::string type;
                ss >> type;
                if (type == "cp") ss >> l_score_cp;
                else if (type == "mate") ss >> l_score_mate;
            }
            else if (token == "wdl") {
                ss >> l_wdl_w >> l_wdl_d >> l_wdl_l;
            }
            else if (token == "pv") reading_pv = true;
        }
        
        {
            std::lock_guard<std::mutex> lock(info_mutex);
            
            if (l_depth > 0) current_info.depth = l_depth;
            if (l_nps > 0) current_info.nps = l_nps;
            if (l_nodes > 0) current_info.nodes = l_nodes;
            
            AnalysisInfo::LineInfo& li = current_info.lines[l_multipv];
            li.score_cp = l_score_cp;
            li.score_mate = l_score_mate;
            li.wdl_w = l_wdl_w;
            li.wdl_d = l_wdl_d;
            li.wdl_l = l_wdl_l;
            if (!l_pv.empty()) li.pv = l_pv;
            
            if (l_multipv == 1) {
                current_info.score_cp = l_score_cp;
                current_info.score_mate = l_score_mate;
                current_info.wdl_w = l_wdl_w;
                current_info.wdl_d = l_wdl_d;
                current_info.wdl_l = l_wdl_l;
                if (!l_pv.empty()) {
                    current_info.pv = l_pv;
                    size_t space = l_pv.find(' ');
                    current_info.best_move = (space != std::string::npos) ? l_pv.substr(0, space) : l_pv;
                }
            }
        }
    }
};
