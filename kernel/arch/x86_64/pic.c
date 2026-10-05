#include "pic.h"
#include <voxi/io.h>

#define PIC1 0x20
#define PIC2 0xA0
#define PIC1_DATA (PIC1 + 1)
#define PIC2_DATA (PIC2 + 1)
#define PIC_EOI 0x20

#define ICW1_INIT 0x10
#define ICW1_ICW4 0x01
#define ICW4_8086 0x01

void pic_remap(void) {
    uint8_t mask1 = inb(PIC1_DATA);
    uint8_t mask2 = inb(PIC2_DATA);

    outb(PIC1, ICW1_INIT | ICW1_ICW4);
    io_wait();
    outb(PIC2, ICW1_INIT | ICW1_ICW4);
    io_wait();

    outb(PIC1_DATA, 32);     /* master offset -> vector 32 */
    io_wait();
    outb(PIC2_DATA, 40);     /* slave offset -> vector 40 */
    io_wait();

    outb(PIC1_DATA, 4);      /* master: slave on IRQ2 */
    io_wait();
    outb(PIC2_DATA, 2);      /* slave: cascade identity */
    io_wait();

    outb(PIC1_DATA, ICW4_8086);
    io_wait();
    outb(PIC2_DATA, ICW4_8086);
    io_wait();

    outb(PIC1_DATA, mask1);
    outb(PIC2_DATA, mask2);
}

void pic_send_eoi(uint8_t irq) {
    if (irq >= 8) outb(PIC2, PIC_EOI);
    outb(PIC1, PIC_EOI);
}

void pic_set_mask(uint8_t irq) {
    uint16_t port = irq < 8 ? PIC1_DATA : PIC2_DATA;
    uint8_t bit = irq < 8 ? irq : (uint8_t)(irq - 8);
    uint8_t val = inb(port) | (uint8_t)(1 << bit);
    outb(port, val);
}

void pic_clear_mask(uint8_t irq) {
    uint16_t port = irq < 8 ? PIC1_DATA : PIC2_DATA;
    uint8_t bit = irq < 8 ? irq : (uint8_t)(irq - 8);
    uint8_t val = (uint8_t)(inb(port) & ~(1 << bit));
    outb(port, val);
}
