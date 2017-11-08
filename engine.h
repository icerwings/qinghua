
#include <stdint.h>
#include "kfifo.h"

class Engine {
public:
    explicit Engine(const string & db, uint32_t poolSize) {
        m_fifo = new Kfifo<MYSQL *>(poolSize);
        if (m_fifo == nullptr) {
            return;
        }
        for (int i = 0; i < poolSize; i++) {
            m_fifo->Enqueue(nullptr);
        }
    }
    
    ~Engine() {
        if (m_fifo != nullptr) {
            delete m_fifo;
        }
    }

    

private:
    Kfifo<MYSQL *>          *m_fifo;
};

