#ifndef UI_CONTROLLER_H
#define UI_CONTROLLER_H

#include "pipe_manager.h"
#include "compress_manager.h"
#include "file_manager.h"
#include "search_engine.h"
#include "network_manager.h"
#include <iostream>
#include <limits>
#include <iomanip>
#include <set>

using namespace std;

class UIController {
private:
    PipeManager& pipeManager;
    CompressManager& compressManager;
    Logger& logger;
    FileManager& fileManager;
    SearchEngine searchEngine;
    NetworkManager networkManager;

    // Список допустимых диаметров
    const set<int> ALLOWED_DIAMETERS = {500, 700, 1000, 1400};

public:
    UIController(PipeManager& pm, CompressManager& cm, Logger& log, FileManager& fm)
        : pipeManager(pm), compressManager(cm), logger(log), fileManager(fm), 
          searchEngine(log), networkManager(pm, cm) {}

    // --- ОБНОВЛЕННЫЙ МЕТОД: AddPipe (с проверкой диаметров) ---
    void AddPipe() {
        Pipe pipe = {};
        cout << "\nPipe parameters:\n";
        cout << "Enter KM mark (name): "; cin.ignore(); getline(cin, pipe.km_mark);

        cout << "Enter length (km): "; cin >> pipe.length;
        if (cin.fail() || pipe.length <= 0) {
            cout << "Error: Invalid length.\n"; cin.clear(); cin.ignore(10000, '\n'); return;
        }

        // Ввод диаметра с проверкой
        while (true) {
            cout << "Enter diameter (Allowed: 500, 700, 1000, 1400): ";
            cin >> pipe.diametr;
            if (cin.fail()) {
                cin.clear(); cin.ignore(10000, '\n'); cout << "Invalid input.\n"; continue;
            }
            if (ALLOWED_DIAMETERS.count(pipe.diametr)) {
                break;
            } else {
                cout << "Error: Diameter not allowed.\n";
            }
        }

        cout << "On repair? (0 - no, 1 - yes): "; cin >> pipe.repair;

        pipeManager.Add(pipe);
        cout << "Pipe added successfully!\n";
        logger.Log("ADDED PIPE manually");
    }

    // Метод для создания трубы при соединении (тоже с валидацией, но диаметр уже передан)
    int AddPipeInteractive(int fixedDiameter) {
        // Проверка на всякий случай, если вызов будет программным
        if (ALLOWED_DIAMETERS.find(fixedDiameter) == ALLOWED_DIAMETERS.end()) {
             cout << "Error: Invalid diameter passed to interactive creation.\n";
             return -1;
        }

        Pipe pipe = {};
        pipe.diametr = fixedDiameter;
        
        cout << "\n--- Creating new pipe (Diameter: " << fixedDiameter << " mm) ---\n";
        cout << "Enter KM mark (name): "; cin.ignore(); getline(cin, pipe.km_mark);
        cout << "Enter length (km): "; cin >> pipe.length;
        
        if (cin.fail() || pipe.length <= 0) {
            cout << "Error: Invalid length.\n"; cin.clear(); cin.ignore(10000, '\n'); return -1;
        }

        cout << "On repair? (0/1): "; cin >> pipe.repair;

        pipeManager.Add(pipe);
        return pipeManager.GetAll().back().id;
    }

    // --- МЕТОДЫ ДЛЯ СЕТИ ---

    void ConnectNetwork() {
        cout << "\n===== Connect Network Elements =====\n";
        int srcId, destId, diameter;
        cout << "Enter Source CS ID: "; cin >> srcId;
        cout << "Enter Destination CS ID: "; cin >> destId;
        
        if (srcId == destId) { cout << "Error: Loops not allowed.\n"; return; }
        if (!compressManager.FindById(srcId) || !compressManager.FindById(destId)) {
            cout << "Error: CS not found.\n"; return;
        }

        while (true) {
            cout << "Enter Pipe Diameter (500, 700, 1000, 1400): ";
            cin >> diameter;
            if (ALLOWED_DIAMETERS.count(diameter)) break;
            cout << "Invalid diameter.\n"; cin.clear(); cin.ignore(10000, '\n');
        }

        int pipeId = pipeManager.FindFreePipeID(diameter);

        if (pipeId != -1) {
            cout << "Found free pipe ID: " << pipeId << ". Connecting...\n";
            pipeManager.LinkPipe(pipeId, srcId, destId);
            logger.Log("CONNECTED CS" + to_string(srcId) + " -> CS" + to_string(destId));
        } else {
            cout << "No free pipe found. Creating new...\n";
            pipeId = AddPipeInteractive(diameter);
            if (pipeId != -1) {
                pipeManager.LinkPipe(pipeId, srcId, destId);
                logger.Log("CONNECTED (NEW) CS" + to_string(srcId) + " -> CS" + to_string(destId));
            }
        }
    }

    void CalculatePath() {
        cout << "\n===== Shortest Path Calculation =====\n";
        int start, end;
        cout << "Enter Start CS ID: "; cin >> start;
        cout << "Enter End CS ID: "; cin >> end;
        networkManager.FindShortestPath(start, end);
    }

    void CalculateFlow() {
        cout << "\n===== Max Flow Calculation =====\n";
        int start, end;
        cout << "Enter Source CS ID: "; cin >> start;
        cout << "Enter Sink CS ID: "; cin >> end;
        networkManager.CalculateMaxFlow(start, end);
    }

    void DisconnectNetwork() {
        networkManager.DisplayNetwork();
        cout << "Enter Pipe ID to disconnect: ";
        int id; cin >> id;
        networkManager.DisconnectPipe(id);
        logger.Log("DISCONNECTED Pipe " + to_string(id));
    }

    void PerformTopologicalSort() {
        vector<int> res = networkManager.TopologicalSort();
        if(!res.empty()) {
            cout << "Topological Order: ";
            for(int id : res) cout << id << " ";
            cout << endl;
        }
    }

    void ViewNetwork() { networkManager.DisplayNetwork(); }

    // --- Standard Wrappers ---
    void AddCompress() { 
        Compress c;
        cout << "Enter name: "; cin.ignore(); getline(cin, c.name);
        cout << "Workshops: "; cin >> c.workshop_count;
        cout << "Working: "; cin >> c.workshop_working;
        cout << "Class: "; cin.ignore(); getline(cin, c.classification);
        cout << "Status (0/1): "; cin >> c.working;
        compressManager.Add(c);
        logger.Log("ADDED CS");
    }
    
    // Остальные методы UI без изменений...
    void ViewAllPipes() {
        auto pipes = pipeManager.GetAll();
        for(auto& p : pipes) cout << "ID:" << p.id << " L:" << p.length << " D:" << p.diametr << (p.source_cs_id ? " [LINKED]" : "") << endl;
    }
    void ViewAllCompress() {
        auto cs = compressManager.GetAll();
        for(auto& c : cs) cout << "ID:" << c.id << " " << c.name << endl;
    }
    void EditPipeById() { int id; cout << "ID: "; cin >> id; Pipe* p = pipeManager.FindById(id); if(p) { cout << "New repair status (0/1): "; cin >> p->repair; } }
    void EditCompressById() { int id; cout << "ID: "; cin >> id; Compress* c = compressManager.FindById(id); if(c) { cout << "New name: "; cin.ignore(); getline(cin, c->name); } }
    void DeletePipe() { int id; cout << "ID: "; cin >> id; pipeManager.Delete(id); }
    void DeleteCompress() { int id; cout << "ID: "; cin >> id; compressManager.Delete(id); }
    void SearchPipes() { searchEngine.SearchPipesById(pipeManager.GetAll(), 1); } // Заглушка, используй свой код
    void SearchCompress() { searchEngine.SearchCompressById(compressManager.GetAll(), 1); } // Заглушка
    void SaveData() { fileManager.SaveAllData(pipeManager, compressManager); }
    void LoadData(int& p, int& c) { fileManager.LoadAllData(pipeManager, compressManager, p, c); }
    void ViewLogs() { logger.ViewLogs(); }
};

#endif