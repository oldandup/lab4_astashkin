#ifndef NETWORK_MANAGER_H
#define NETWORK_MANAGER_H

#include "pipe_manager.h"
#include "compress_manager.h"
#include <vector>
#include <map>
#include <set>
#include <algorithm>
#include <iostream>
#include <cmath>
#include <queue>
#include <limits>
#include <functional>

using namespace std;

class NetworkManager {
private:
    PipeManager& pipeManager;
    CompressManager& compressManager;

public:
    NetworkManager(PipeManager& pm, CompressManager& cm) 
        : pipeManager(pm), compressManager(cm) {}

    // --- ВСПОМОГАТЕЛЬНЫЕ ФОРМУЛЫ ---

    // Расчет пропускной способности трубы
    // Формула: sqrt(d^5 / l). 
    // Результат делим на коэффициент, чтобы цифры были читаемыми (условные единицы)
    double CalculateCapacity(const Pipe& p) {
        if (p.repair) return 0.0; // Если в ремонте, поток 0
        
        // Диаметр в мм, длина в км. 
        // Для физической точности нужно приводить к одной СИ, но для оценки достаточно пропорции.
        // d^5 растет очень быстро, используем double.
        double val = sqrt(pow(p.diametr, 5) / p.length);
        return round(val / 100.0); // Скалируем для удобства чтения
    }

    // Расчет веса ребра (для кратчайшего пути)
    double CalculateWeight(const Pipe& p) {
        if (p.repair) return numeric_limits<double>::infinity(); // Бесконечный путь
        return p.length;
    }

    // --- ОТОБРАЖЕНИЕ ---
    void DisplayNetwork() {
        cout << "\n===== Gas Transport Network =====\n";
        bool hasConnections = false;
        
        auto& stations = compressManager.GetAll();
        map<int, string> csNames;
        for(const auto& cs : stations) csNames[cs.id] = cs.name;

        for (const auto& pipe : pipeManager.GetAll()) {
            if (pipe.source_cs_id != 0 && pipe.dest_cs_id != 0) {
                cout << "CS " << pipe.source_cs_id << " -> CS " << pipe.dest_cs_id
                     << " | Pipe ID: " << pipe.id 
                     << ", L: " << pipe.length << "km"
                     << ", D: " << pipe.diametr << "mm"
                     << (pipe.repair ? " [REPAIR]" : "")
                     << " | MaxFlow: " << CalculateCapacity(pipe) 
                     << "\n";
                hasConnections = true;
            }
        }
        if (!hasConnections) cout << "No active connections.\n";
    }

    // --- АЛГОРИТМ 1: КРАТЧАЙШИЙ ПУТЬ (Дейкстра) ---
    void FindShortestPath(int startId, int endId) {
        map<int, vector<int>> adjPipes; // CS_ID -> Список ID труб, исходящих из неё
        
        // Строим граф смежности
        for (const auto& pipe : pipeManager.GetAll()) {
            if (pipe.source_cs_id != 0 && pipe.dest_cs_id != 0 && !pipe.repair) {
                adjPipes[pipe.source_cs_id].push_back(pipe.id);
            }
        }

        // dist[id] - минимальное расстояние от старта до id
        map<int, double> dist;
        map<int, int> parentPipe; // Какую трубу использовали, чтобы прийти
        map<int, int> parentNode; // Из какого узла пришли

        // Инициализация бесконечностью
        for (const auto& cs : compressManager.GetAll()) {
            dist[cs.id] = numeric_limits<double>::infinity();
        }

        if (dist.find(startId) == dist.end() || dist.find(endId) == dist.end()) {
            cout << "Error: Start or End CS ID not found.\n";
            return;
        }

        dist[startId] = 0;
        // Priority Queue хранит пары <Distance, NodeID>, сортировка по возрастанию (через greater)
        priority_queue<pair<double, int>, vector<pair<double, int>>, greater<pair<double, int>>> pq;
        pq.push({0, startId});

        while (!pq.empty()) {
            double d = pq.top().first;
            int u = pq.top().second;
            pq.pop();

            if (d > dist[u]) continue;
            if (u == endId) break; // Дошли до цели

            // Проходим по всем трубам, выходящим из u
            for (int pipeId : adjPipes[u]) {
                Pipe* p = pipeManager.FindById(pipeId);
                int v = p->dest_cs_id;
                double weight = p->length;

                if (dist[u] + weight < dist[v]) {
                    dist[v] = dist[u] + weight;
                    parentPipe[v] = pipeId;
                    parentNode[v] = u;
                    pq.push({dist[v], v});
                }
            }
        }

        // Восстановление пути
        if (dist[endId] == numeric_limits<double>::infinity()) {
            cout << "\nResult: No path exists between CS " << startId << " and CS " << endId << ".\n";
        } else {
            cout << "\n===== Shortest Path Result =====\n";
            cout << "Total Length: " << dist[endId] << " km\n";
            cout << "Path: ";
            
            vector<int> path;
            int curr = endId;
            while (curr != startId) {
                path.push_back(curr);
                curr = parentNode[curr];
            }
            path.push_back(startId);
            reverse(path.begin(), path.end());

            for (size_t i = 0; i < path.size(); i++) {
                cout << path[i];
                if (i < path.size() - 1) {
                    // Находим трубу между узлами для красоты вывода
                    cout << " --(Pipe " << parentPipe[path[i+1]] << ")--> ";
                }
            }
            cout << "\n";
        }
    }

    // --- АЛГОРИТМ 2: МАКСИМАЛЬНЫЙ ПОТОК (Эдмондс-Карп) ---
    void CalculateMaxFlow(int source, int sink) {
        // Строим матрицу пропускных способностей
        // capacity[u][v] = суммарная пропускная способность труб от u к v
        map<int, map<int, double>> capacity;
        // Список смежности для BFS (включает и обратные ребра с 0 емкостью изначально)
        map<int, vector<int>> graph;

        // Заполняем граф
        for (const auto& pipe : pipeManager.GetAll()) {
            if (pipe.source_cs_id != 0 && pipe.dest_cs_id != 0) {
                int u = pipe.source_cs_id;
                int v = pipe.dest_cs_id;
                double cap = CalculateCapacity(pipe);
                
                if (cap > 0) {
                    if (capacity[u][v] == 0 && capacity[v][u] == 0) {
                        graph[u].push_back(v);
                        graph[v].push_back(u); // Для остаточного графа
                    }
                    capacity[u][v] += cap;
                }
            }
        }

        if (graph.find(source) == graph.end() || graph.find(sink) == graph.end()) {
             cout << "Error: Source or Sink not connected to network.\n";
             return;
        }

        double maxFlow = 0;
        
        while (true) {
            // BFS для поиска пути в остаточном графе
            map<int, int> parent;
            map<int, bool> visited;
            queue<int> q;
            
            q.push(source);
            visited[source] = true;
            parent[source] = -1;

            bool pathFound = false;
            while (!q.empty()) {
                int u = q.front();
                q.pop();

                if (u == sink) {
                    pathFound = true;
                    break;
                }

                for (int v : graph[u]) {
                    if (!visited[v] && capacity[u][v] > 0) {
                        parent[v] = u;
                        visited[v] = true;
                        q.push(v);
                    }
                }
            }

            if (!pathFound) break;

            // Ищем минимальную пропускную способность на найденном пути
            double pathFlow = numeric_limits<double>::infinity();
            int curr = sink;
            while (curr != source) {
                int prev = parent[curr];
                pathFlow = min(pathFlow, capacity[prev][curr]);
                curr = prev;
            }

            // Обновляем остаточный граф
            curr = sink;
            while (curr != source) {
                int prev = parent[curr];
                capacity[prev][curr] -= pathFlow;
                capacity[curr][prev] += pathFlow;
                curr = prev;
            }

            maxFlow += pathFlow;
        }

        cout << "\n===== Max Flow Result =====\n";
        cout << "Max Flow from CS " << source << " to CS " << sink << ": " << maxFlow << " (approx. units)\n";
    }

    // --- Топологическая сортировка (оставляем для совместимости) ---
    vector<int> TopologicalSort() {
        map<int, vector<int>> adj;
        set<int> nodes;
        for (const auto& pipe : pipeManager.GetAll()) {
            if (pipe.source_cs_id != 0 && pipe.dest_cs_id != 0) {
                adj[pipe.source_cs_id].push_back(pipe.dest_cs_id);
                nodes.insert(pipe.source_cs_id);
                nodes.insert(pipe.dest_cs_id);
            }
        }

        if (nodes.empty()) return {};

        map<int, int> visited; 
        vector<int> result;
        bool hasCycle = false;

        function<void(int)> dfs = [&](int u) {
            visited[u] = 1; 
            for (int v : adj[u]) {
                if (visited[v] == 1) { hasCycle = true; return; }
                if (visited[v] == 0) {
                    dfs(v);
                    if (hasCycle) return;
                }
            }
            visited[u] = 2; 
            result.push_back(u);
        };

        for (int node : nodes) {
            if (visited[node] == 0) {
                dfs(node);
                if (hasCycle) break;
            }
        }

        if (hasCycle) {
            cout << "\nERROR: Cycle detected! Topo sort impossible.\n";
            return {};
        }
        reverse(result.begin(), result.end());
        return result;
    }
    
    void DisconnectPipe(int pipeId) { pipeManager.UnlinkPipe(pipeId); }
};

#endif