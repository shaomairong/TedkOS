#ifndef _KB_TERM_DRIVER_H
#define _KB_TERM_DRIVER_H

#include <stdint.h>
#include <stddef.h>

#include <inc/klibs/spinlock.h>
#include <inc/klibs/lib.h>
#include <inc/i8259.h>

#include <inc/drivers/common.h>

#include <inc/fs/filesystem.h>
#include <inc/fs/fops.h>

namespace KeyB
{
    class IEvent
    {
    public:
        virtual void key(uint32_t kkc, bool capslock) = 0;

        // Down and Up cuts changes to ONE single key at a time.
        virtual void keyDown(uint32_t kkc, bool capslock) = 0;
        virtual void keyUp(uint32_t kkc, bool capslock) = 0;
    };
    class FOps : public IFOps
    {
    public:
        int32_t read(filesystem::File& fdEntity, uint8_t *buf, int32_t bytes);
        int32_t write(filesystem::File& fdEntity, const uint8_t *buf, int32_t bytes);
    };
}

typedef struct
{
    char displayed_char;
    
    //offset is the space this item took up on screen
    // x_offset is always positive.
    // For normal characters, x_offset = 1
    // !!! If newline occurs:
    //  x_offset = SCREEN_WIDTH - old_x
    uint32_t x_offset;

    // y_offset > 0 if and only if ONE new line occur
    // Otherwise (=0) no new line.
    uint8_t y_offset;
} term_buf_item;

namespace Term
{
    class TermPainter
    {
    public:
        virtual void clearScreen() = 0;
        virtual void scrollDown() = 0;
        virtual void setCursor() = 0;
        virtual void showChar() = 0;
    };
    class TextModePainter : public TermPainter
    {
    public:
        virtual bool show();
        virtual bool hide();
    };
    class FOps : public IFOps
    {
    public:
        int32_t read(filesystem::File& fdEntity, uint8_t *buf, int32_t bytes);
        int32_t write(filesystem::File& fdEntity, const uint8_t *buf, int32_t bytes);
    };
    class Term : public KeyB::IEvent
    {
    protected:
        static const size_t bufSize = 128;
        term_buf_item termBuf[bufSize];

        virtual TermPainter* getTermPainter() = 0;
    public:
        Term();

        virtual void key(uint32_t kkc, bool capslock) final;
        virtual void keyDown(uint32_t kkc, bool capslock) final;
        virtual void keyUp(uint32_t kkc, bool capslock) final;

        // These are used by printf in klibs
        // calling these operations WILL clear the buffer.
        virtual void putc(uint8_t c) final;
        virtual void cls() final;

        // The following will be called by Term::FOps and KeyB::FOps
        virtual int32_t read(filesystem::File& fdEntity, uint8_t *buf, int32_t bytes) final;
        virtual int32_t write(filesystem::File& fdEntity, const uint8_t *buf, int32_t bytes) final;

        virtual bool isOwner(int32_t upid) final;
        // setOwner will not block.
        virtual void setOwner(int32_t upid) final;
    };

    class TextTerm : public Term
    {
    public:
        TextTerm();
        bool show();
        bool hide();
    };
}

namespace KeyB
{
    class TextOnlyKbClients
    {
        static const size_t numClients = 4;

        // To support GUI, here we do NOT directly use TermImpl as type of clients
        IEvent* clients[numClients];
        TextTerm clientImpl[numClients];
    public:
        TextOnlyKbClients();
        virtual ~TextOnlyKbClients();

        IEvent* operator [] (size_t i);
    };
    // This filter is executed before any other IEvent.
    //  it will:
    //  1. prevent typing by not sending events to terminal,
    //          after too many chars are typed in.
    //  2. handler shortcuts
    //  3. Otherwise, pass event to all terminals
    //  4. It always handles the kb_read buffer.
    class BasicShortcutFilter
    {
        friend int kb_handler(int irq, unsigned int saved_reg);
    private:
        static size_t currClient = 0;

        static bool caps_locked = false;

        // This field is used to tell whether COMBINATION key is pressed
        //      If so, then it also contains all COMBINATION keys pressed.
        //      Note that pending_kc's MSB is never 1 (released)
        static uint32_t pending_kc = 0;

        // The ENTRY POINT into C++ world for all keyboard events.
        static void handle(uint32_t kernelKeycode);

        // Helper for "handle". true = this key should NOT be passed to IEvent::key()
        static bool handleShortcut(uint32_t kernelKeycode);
    public:
        // Call init() before using Keyboard or Terminal!
        static void init();

        static void setCurrClient(size_t client);
        static IEvent* getCurrClient();
    };
}

#define NUM_TERMINALS           1
#define TERM_BUFFER_SIZE        128
extern term_buf_item term_buf[TERM_BUFFER_SIZE];
extern volatile int32_t term_buf_pos;

// Must lock term_lock when accessing:
//  0. terminal
//  1. the term_read_buffer
//  2. isThisTerminalInUse
//  2. isThisTerminalWaitingForEnter
extern spinlock_t term_lock;

DEFINE_DRIVER_INIT(term);
DEFINE_DRIVER_REMOVE(term);


// These implementation will make sure cursors are moved so that kernel's output fits well with user's input

// By default the klib's printf and clear functions use this implementation.
// Change the macro redirection there to switch to old, given versions

// clear screen
void term_cls(void);

// Print one char. Must be either printable or newline character
void term_putc(uint8_t c);

extern spinlock_t keyboard_lock;

DEFINE_DRIVER_INIT(kb);
DEFINE_DRIVER_REMOVE(kb);


#endif//_KB_TERM_DRIVER_H

