/*
** EntityCore
** C++ Tools - Tracer
** File description:
** Dynamic variable tracer
** License:
** MIT (see https://github.com/Calvin-Ruiz/EntityCore)
*/
#include "Tracer.hpp"
#include <chrono>

Tracer::Tracer(int width, int height) : width(width), height(height)
{}

Tracer::~Tracer()
{
    stop();
}

void Tracer::emplace(Trace type, void *data, const std::string &name, trace_drawer_t handler)
{
    interrupt = true;
    while (!interrupted)
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    tracing.push_back({handler, data, name, type});
    interrupt = false;
}

void Tracer::erase(const std::string &name)
{
    interrupt = true;
    while (!interrupted)
        std::this_thread::sleep_for(std::chrono::milliseconds(2));

    for (unsigned int i = 0; i < tracing.size(); ++i) {
        if (name == tracing[i].name) {
            if (i < lastSize) {
                *(pbuffer++) = '\x1b';
                *(pbuffer++) = '[';
                putNbr(i + 1);
                *(pbuffer++) = 'd';
                *(pbuffer++) = '\x1b';
                *(pbuffer++) = '[';
                *(pbuffer++) = 'M';
                --lastSize;
            }
            tracing.erase(tracing.begin() + i);
        }
    }
    interrupt = false;
}
// \xe2\x94 ,- 8c - 80 -, 90 '- 94 -' 98 | 82
void Tracer::drawBorders()
{
    const char clr[2] = {27, 'c'};
    char str[3] = {(char) 226, (char) 148, (char) 140};
    char end[3] = {'\x1b', '[', 'H'};

    std::cout.write(clr, 2);
    std::cout.write(str, 3);
    str[2] = 128;
    for (short i = 1; ++i < width;)
        std::cout.write(str, 3);
    str[2] = 144;
    std::cout.write(str, 3);
    std::cout.write("\n", 1);
    str[2] = 148;
    std::cout.write(str, 3);
    str[2] = 128;
    for (short i = 1; ++i < width;)
        std::cout.write(str, 3);
    str[2] = 152;
    std::cout.write(str, 3);
    std::cout.write(end, 3);
}

void Tracer::start()
{
    alive = true;
    lastSize = 0;
    drawBorders();
    thread = std::thread(&Tracer::mainloop, this);
}

void Tracer::stop()
{
    alive = false;
    if (thread.joinable())
        thread.join();
}

void Tracer::mainloop()
{
    auto delay = std::chrono::duration<int, std::ratio<1,1000000>>(1000000/60);
    auto clock = std::chrono::system_clock::now();

    interrupted = false;
    while (alive) {
        clock += delay;
        while (interrupt) {
            interrupted = true;
            while (interrupt)
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            interrupted = false;
        }
        if (needRedraw) {
            lastSize = 0;
            drawBorders();
            needRedraw = false;
        }
        draw();
        std::this_thread::sleep_until(clock);
    }
    interrupted = true;
}

void Tracer::draw()
{
    pbuffer = buffer;
    for (unsigned int i = 0; i < lastSize; ++i) {
        subdraw(tracing[i], i + 2);
    }
    while (lastSize < tracing.size()) {
        insert(tracing[lastSize++]);
    }
    std::cout.write(reinterpret_cast<char *>(buffer), (int64_t) (pbuffer - buffer));
    std::cout.flush();
}

void Tracer::insert(TraceData &data)
{
    *(pbuffer++) = '\n';
    *(pbuffer++) = '\x1b';
    *(pbuffer++) = '[';
    *(pbuffer++) = 'L';
    *(pbuffer++) = 226;
    *(pbuffer++) = 148;
    *(pbuffer++) = 130;
    if (data.name.empty()) {
        switch (data.type) {
            case Trace::CHAR:
                putStr("char\t=");
                break;
            case Trace::UCHAR:
                putStr("uchar\t=");
                break;
            case Trace::SHORT:
                putStr("short\t=");
                break;
            case Trace::USHORT:
                putStr("ushort\t=");
                break;
            case Trace::INT:
                putStr("int\t=");
                break;
            case Trace::UINT:
                putStr("uint\t=");
                break;
            case Trace::LONG:
                putStr("long\t=");
                break;
            case Trace::ULONG:
                putStr("ulong\t=");
                break;
            case Trace::FLOAT:
                putStr("float\t=");
                break;
            case Trace::DOUBLE:
                putStr("double\t=");
                break;
            case Trace::PTR:
                putStr("ptr\t=");
                break;
            case Trace::STRING:
                putStr("string\t=");
                break;
            case Trace::CUSTOM:
                putStr("custom\t=");
                break;
        }
    } else {
        *(pbuffer++) = '\x1b';
        *(pbuffer++) = '[';
        *(pbuffer++) = '3';
        *(pbuffer++) = '4';
        *(pbuffer++) = 'm';
        if (data.name.size() <= 6) {
            putStr(data.name.c_str());
            *(pbuffer++) = '\t';
        } else {
            *(pbuffer++) = data.name[0];
            *(pbuffer++) = data.name[1];
            *(pbuffer++) = data.name[2];
            *(pbuffer++) = data.name[3];
            *(pbuffer++) = data.name[4];
            *(pbuffer++) = data.name[5];
            *(pbuffer++) = data.name[6];
        }
        *(pbuffer++) = '\x1b';
        *(pbuffer++) = '[';
        *(pbuffer++) = '9';
        *(pbuffer++) = '5';
        *(pbuffer++) = 'm';
        *(pbuffer++) = '=';
        *(pbuffer++) = '\x1b';
        *(pbuffer++) = '[';
        *(pbuffer++) = '0';
        *(pbuffer++) = 'm';
    }
    subdraw(data, lastSize + 1);
}

void Tracer::subdraw(TraceData &data, int y)
{
    jmp(11, y);
    switch (data.type) {
        case Trace::CHAR:
            putNbr(*((char *) data.pdata));
            break;
        case Trace::UCHAR:
            putNbr(*((unsigned char *) data.pdata));
            break;
        case Trace::SHORT:
            putNbr(*((short *) data.pdata));
            break;
        case Trace::USHORT:
            putNbr(*((unsigned short *) data.pdata));
            break;
        case Trace::INT:
            putNbr(*((int *) data.pdata));
            break;
        case Trace::UINT:
            putNbr<unsigned int>(*((unsigned int *) data.pdata));
            break;
        case Trace::LONG:
            putNbr<uint64_t>(*((int64_t *) data.pdata));
            break;
        case Trace::ULONG:
            putNbr<uint64_t>(*((uint64_t *) data.pdata));
            break;
        case Trace::FLOAT:
            putReal(*((float *) data.pdata));
            break;
        case Trace::DOUBLE:
            putReal(*((double *) data.pdata));
            break;
        case Trace::PTR:
            putPtr(*((uint64_t *) data.pdata));
            break;
        case Trace::STRING:
            *(pbuffer++) = '\"';
            putStr(((char *) data.pdata));
            *(pbuffer++) = '\"';
            break;
        case Trace::CUSTOM:
            pbuffer = data.handler(data.pdata, pbuffer);
            break;
    }
    *(pbuffer++) = '\x1b';
    *(pbuffer++) = '[';
    *(pbuffer++) = 'K';
    jmp(width, y);
    *(pbuffer++) = 226;
    *(pbuffer++) = 148;
    *(pbuffer++) = 130;
}

void Tracer::putStr(const char *str)
{
    while (*str)
        *(pbuffer++) = *(str++);
}

void Tracer::jmp(int x, int y)
{
    *(pbuffer++) = '\x1b';
    *(pbuffer++) = '[';
    putNbr(y);
    *(pbuffer++) = ';';
    putNbr(x);
    *(pbuffer++) = 'H';
}

void Tracer::putPtr(uint64_t ptr)
{
    const char table[] = "0123456789abcdef";
    uint64_t mask = 0xf000000000000000;
    int level = 60;

    *(pbuffer++) = '0';
    *(pbuffer++) = 'x';
    while (!(mask & ptr)) {
        level -= 4;
        mask >>= 4;
    }
    while (mask) {
        *(pbuffer++) = table[(ptr & mask) >> level];
        level -= 4;
        mask >>= 4;
    }
}

void Tracer::redraw()
{
    needRedraw = true;
}
