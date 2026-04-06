#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <float.h>
typedef struct Edge {
    int to_idx;      // индекс узла назначения
    double length;   // длина ребра (вес)
} Edge;
typedef struct Node {
    int id;          // исходный ID из CSV
    double lat;      // широта
    double lon;      // долгота
    Edge *edges;     // динамический массив исходящих рёбер
    int edge_count;  // количество рёбер
    int edge_capacity;
} Node;
typedef struct Graph {
    Node *nodes;         // массив узлов (индекс = внутренний индекс)
    int node_count;      // количество узлов
    int *id_to_idx;      // отображение исходного ID -> внутренний индекс
    int max_id;          // текущий размер id_to_idx (максимальный ID + 1)
} Graph;
void graph_init(Graph *g, int initial_max_id) {
    g->node_count = 0;
    g->max_id = initial_max_id;
    g->nodes = malloc(sizeof(Node) * initial_max_id);
    g->id_to_idx = malloc(sizeof(int) * initial_max_id);
    for (int i = 0; i < initial_max_id; i++) {
        g->id_to_idx[i] = -1;
    }
}
void graph_ensure_id_range(Graph *g, int id) {
    if (id >= g->max_id) {
        int new_max = id + 10000;
        g->id_to_idx = realloc(g->id_to_idx, sizeof(int) * new_max);
        for (int i = g->max_id; i < new_max; i++) {
            g->id_to_idx[i] = -1;
        }
        g->max_id = new_max;
        g->nodes = realloc(g->nodes, sizeof(Node) * new_max);
    }
}
int graph_add_node(Graph *g, int id, double lat, double lon) {
    graph_ensure_id_range(g, id);
    int idx = g->node_count;
    g->nodes[idx].id = id;
    g->nodes[idx].lat = lat;
    g->nodes[idx].lon = lon;
    g->nodes[idx].edges = NULL;
    g->nodes[idx].edge_count = 0;
    g->nodes[idx].edge_capacity = 0;
    g->id_to_idx[id] = idx;
    g->node_count++;
    return idx;
}
void graph_add_edge(Graph *g, int from_idx, int to_idx, double length) {
    Node *node = &g->nodes[from_idx];
    if (node->edge_count == node->edge_capacity) {
        node->edge_capacity = (node->edge_capacity == 0) ? 4 : node->edge_capacity * 2;
        node->edges = realloc(node->edges, sizeof(Edge) * node->edge_capacity);
    }
    node->edges[node->edge_count].to_idx = to_idx;
    node->edges[node->edge_count].length = length;
    node->edge_count++;
}
int graph_get_idx(Graph *g, int id) {
    if (id < 0 || id >= g->max_id) return -1;
    return g->id_to_idx[id];
}
void graph_free(Graph *g) {
    for (int i = 0; i < g->node_count; i++) {
        free(g->nodes[i].edges);
    }
    free(g->nodes);
    free(g->id_to_idx);
}
int read_nodes(Graph *g, const char *filename) {
    FILE *f = fopen(filename, "r");
    if (!f) {
        fprintf(stderr, "Ошибка: не удалось открыть %s\n", filename);
        return -1;
    }
    char line[1024];
    if (!fgets(line, sizeof(line), f)) {
        fclose(f);
        return -1;
    }
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\n")] = 0;
        int id;
        double lat, lon;
        if (sscanf(line, "%d,%lf,%lf", &id, &lat, &lon) == 3) {
            graph_add_node(g, id, lat, lon);
        } else {
            fprintf(stderr, "Предупреждение: пропущена некорректная строка в nodes.csv: %s\n", line);
        }
    }
    fclose(f);
    return 0;
}

int read_edges(Graph *g, const char *filename) {
    FILE *f = fopen(filename, "r");
    if (!f) {
        fprintf(stderr, "Ошибка: не удалось открыть %s\n", filename);
        return -1;
    }
    char line[1024];
    if (!fgets(line, sizeof(line), f)) {
        fclose(f);
        return -1;
    }
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\n")] = 0;
        int from_id, to_id, oneway;
        double length;
        char name[256] = {0};
        int n = sscanf(line, "%d,%d,%lf,%d,%255[^\n]", &from_id, &to_id, &length, &oneway, name);
        if (n < 4) {
            fprintf(stderr, "Предупреждение: пропущена некорректная строка в edges.csv: %s\n", line);
            continue;
        }
        int from_idx = graph_get_idx(g, from_id);
        int to_idx = graph_get_idx(g, to_id);
        if (from_idx == -1 || to_idx == -1) {
            fprintf(stderr, "Предупреждение: ребро ссылается на несуществующий узел (%d -> %d)\n", from_id, to_id);
            continue;
        }
        graph_add_edge(g, from_idx, to_idx, length);
        if (!oneway) {
            graph_add_edge(g, to_idx, from_idx, length);
        }
    }
    fclose(f);
    return 0;
}
int find_closest_node(Graph *g, double lat, double lon) {
    int best_idx = -1;
    double best_dist2 = INFINITY;
    for (int i = 0; i < g->node_count; i++) {
        double dx = g->nodes[i].lat - lat;
        double dy = g->nodes[i].lon - lon;
        double d2 = dx * dx + dy * dy;
        if (d2 < best_dist2) {
            best_dist2 = d2;
            best_idx = i;
        }
    }
    return best_idx;
}
typedef struct {
    int node_idx;
    double dist;
} HeapItem;

typedef struct {
    HeapItem *items;
    int size;
    int capacity;
} MinHeap;

void heap_init(MinHeap *h, int capacity) {
    h->items = malloc(sizeof(HeapItem) * capacity);
    h->size = 0;
    h->capacity = capacity;
}

void heap_push(MinHeap *h, int node_idx, double dist) {
    if (h->size == h->capacity) {
        h->capacity *= 2;
        h->items = realloc(h->items, sizeof(HeapItem) * h->capacity);
    }
    int i = h->size++;
    h->items[i] = (HeapItem){node_idx, dist};
    while (i > 0) {
        int parent = (i - 1) / 2;
        if (h->items[parent].dist <= h->items[i].dist)
            break;
        HeapItem tmp = h->items[parent];
        h->items[parent] = h->items[i];
        h->items[i] = tmp;
        i = parent;
    }
}

HeapItem heap_pop(MinHeap *h) {
    HeapItem top = h->items[0];
    h->items[0] = h->items[--h->size];
    int i = 0;
    while (1) {
        int left = 2 * i + 1;
        int right = 2 * i + 2;
        int smallest = i;
        if (left < h->size && h->items[left].dist < h->items[smallest].dist)
            smallest = left;
        if (right < h->size && h->items[right].dist < h->items[smallest].dist)
            smallest = right;
        if (smallest == i)
            break;
        HeapItem tmp = h->items[i];
        h->items[i] = h->items[smallest];
        h->items[smallest] = tmp;
        i = smallest;
    }
    return top;
}

int heap_empty(MinHeap *h) {
    return h->size == 0;
}

void heap_free(MinHeap *h) {
    free(h->items);
}

void dijkstra(Graph *g, int start_idx, double **dist_out, int **prev_out) {
    int n = g->node_count;
    double *dist = malloc(sizeof(double) * n);
    int *prev = malloc(sizeof(int) * n);
    for (int i = 0; i < n; i++) {
        dist[i] = INFINITY;
        prev[i] = -1;
    }
    dist[start_idx] = 0.0;

    MinHeap heap;
    heap_init(&heap, n);
    heap_push(&heap, start_idx, 0.0);

    while (!heap_empty(&heap)) {
        HeapItem cur = heap_pop(&heap);
        int u = cur.node_idx;
        double d = cur.dist;
        if (d > dist[u]) continue;

        Node *node = &g->nodes[u];
        for (int i = 0; i < node->edge_count; i++) {
            int v = node->edges[i].to_idx;
            double w = node->edges[i].length;
            if (dist[u] + w < dist[v] - 1e-12) {
                dist[v] = dist[u] + w;
                prev[v] = u;
                heap_push(&heap, v, dist[v]);
            }
        }
    }

    heap_free(&heap);
    *dist_out = dist;
    *prev_out = prev;
}

void write_path(Graph *g, int start_idx, int finish_idx, int *prev, const char *outfile) {
    if (prev[finish_idx] == -1 && start_idx != finish_idx) {
        FILE *f = fopen(outfile, "w");
        if (f) fclose(f);
        return;
    }
    int *path = malloc(sizeof(int) * g->node_count);
    int len = 0;
    int cur = finish_idx;
    while (cur != -1) {
        path[len++] = cur;
        cur = prev[cur];
    }
    FILE *f = fopen(outfile, "w");
    if (!f) {
        fprintf(stderr, "Ошибка: не удалось создать выходной файл %s\n", outfile);
        free(path);
        return;
    }
    for (int i = len - 1; i >= 0; i--) {
        Node *node = &g->nodes[path[i]];
        fprintf(f, "%.6lf %.6lf\n", node->lat, node->lon);
    }
    fclose(f);
    free(path);
}

int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Использование: %s <папка_с_данными> <входной_файл> <выходной_файл>\n", argv[0]);
        return 1;
    }

    const char *data_dir = argv[1];
    const char *input_file = argv[2];
    const char *output_file = argv[3];

    char nodes_path[1024], edges_path[1024];
    snprintf(nodes_path, sizeof(nodes_path), "%s/nodes.csv", data_dir);
    snprintf(edges_path, sizeof(edges_path), "%s/edges.csv", data_dir);

    Graph graph;
    graph_init(&graph, 1000000);  // предположим, ID до миллиона

    if (read_nodes(&graph, nodes_path) != 0) {
        graph_free(&graph);
        return 1;
    }
    if (read_edges(&graph, edges_path) != 0) {
        graph_free(&graph);
        return 1;
    }

    if (graph.node_count == 0) {
        fprintf(stderr, "Ошибка: граф не содержит узлов\n");
        graph_free(&graph);
        return 1;
    }
    FILE *in = fopen(input_file, "r");
    if (!in) {
        fprintf(stderr, "Ошибка: не удалось открыть входной файл %s\n", input_file);
        graph_free(&graph);
        return 1;
    }
    double start_lat, start_lon, finish_lat, finish_lon;
    if (fscanf(in, "%lf %lf", &start_lat, &start_lon) != 2 ||
        fscanf(in, "%lf %lf", &finish_lat, &finish_lon) != 2) {
        fprintf(stderr, "Ошибка: некорректный формат входного файла (ожидаются две строки: lat lon)\n");
        fclose(in);
        graph_free(&graph);
        return 1;
    }
    fclose(in);

    int start_idx = find_closest_node(&graph, start_lat, start_lon);
    int finish_idx = find_closest_node(&graph, finish_lat, finish_lon);
    if (start_idx == -1 || finish_idx == -1) {
        fprintf(stderr, "Ошибка: не удалось найти узлы для заданных координат\n");
        FILE *f = fopen(output_file, "w");
        if (f) fclose(f);
        graph_free(&graph);
        return 0;
    }
    double *dist;
    int *prev;
    dijkstra(&graph, start_idx, &dist, &prev);
    write_path(&graph, start_idx, finish_idx, prev, output_file);
    free(dist);
    free(prev);
    graph_free(&graph);

    return 0;
}
