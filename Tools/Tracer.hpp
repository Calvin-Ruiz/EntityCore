/*
** EntityCore
** C++ Tools - Tracer
** File description:
** Dynamic variable tracer
** License:
** MIT (see https://github.com/Calvin-Ruiz/EntityCore)
*/

#ifndef TRACER_HPP_
#define TRACER_HPP_

#include <string>
#include <memory>
#include <vector>
#include <thread>
#include <iostream>
#include <atomic>

enum class Trace : unsigned char {
    CHAR,
    UCHAR,
    SHORT,
    USHORT,
    INT,
    UINT,
    LONG,
    ULONG,
    FLOAT,
    DOUBLE,
    PTR,
    STRING, // C-style string
    CUSTOM // Custom handler
};

typedef unsigned char *(*trace_drawer_t)(void *data, unsigned char *buffer);

struct TraceData {
    trace_drawer_t handler;
    void *pdata;
    std::string name;
    Trace type;
};
// \e[K\n \e[H pour aller à droite \e[0b pour répéter 0 fois le caractère précédent \e[L insert line before this one \e[M erase this line
class Tracer {
public:
    Tracer(int width, int height);
    virtual ~Tracer();
    Tracer(const Tracer &cpy) = delete;
    Tracer &operator=(const Tracer &src) = delete;

    void emplace(Trace type, void *data, const std::string &name = "\0", trace_drawer_t handler = nullptr);
    void erase(const std::string &name);
    void start();
    void stop();
    void redraw();
private:
    void drawBorders();
    void mainloop();
    void draw();
    void subdraw(TraceData &data, int y);
    void insert(TraceData &data);
    void jmp(int x, int y);
    void putStr(const char *str);
    void putPtr(uint64_t ptr);
    bool alive = false;
    bool needRedraw = false;
    std::atomic<bool> interrupt {false};
    std::atomic<bool> interrupted {true};
    std::thread thread;
    unsigned char buffer[8192];
    unsigned char *pbuffer;

    const int width;
    const int height;
    std::vector<TraceData> tracing;
    unsigned int lastSize = 0;

    template <typename T>
    void subPutNbr(T value) {
        if (value > 9)
            subPutNbr<T>(value / 10);
        *(pbuffer++) = '0' + (value % 10);
    }
    template <typename T = int>
    inline void putNbr(T value) {
        if (value < 0) {
            *(pbuffer++) = '-';
            value = -value;
        }
        subPutNbr<T>(value);
    }
    template <typename T>
    inline void putReal(T value) {
        if (value < 0) {
            *(pbuffer++) = '-';
            value = -value;
        }
        int ivalue = value;
        subPutNbr(ivalue);
        value -= ivalue;
        *(pbuffer++) = '.';
        for (int i = 0; i < 6 && value >= 0.000001; ++i) {
            value *= 10;
            *(pbuffer++) = '0' + ((int) value) % 10;
        }
    }
};

#endif /* TRACER_HPP_ */
