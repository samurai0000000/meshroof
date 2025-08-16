/*
 * MeshRoofShell.cxx
 *
 * Copyright (C) 2025, Charles Chiou
 */

#include <esp_timer.h>
#include <esp_system.h>
#include <esp_heap_caps.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <libmeshtastic.h>
#include <serial.h>
#include <MeshRoof.hxx>
#include <MeshRoofShell.hxx>

extern shared_ptr<MeshRoof> meshroof;

MeshRoofShell::MeshRoofShell(shared_ptr<SimpleClient> client)
    : SimpleShell(client)
{
    _help_list.push_back("wcfg");
    _help_list.push_back("disc");
    _help_list.push_back("hb");
}

MeshRoofShell::~MeshRoofShell()
{

}

int MeshRoofShell::printf(const char *format, ...)
{
    int ret = 0;
    va_list ap;

    va_start(ap, format);
    ret = usb_vprintf(format, ap);
    va_end(ap);

    return ret;
}

int MeshRoofShell::rx_ready(void) const
{
    return usb_rx_ready();
}

int MeshRoofShell::rx_read(uint8_t *buf, size_t size)
{
    return usb_rx_read_timeout(buf, size, pdMS_TO_TICKS(100));
}

int MeshRoofShell::system (int argc, char **argv)
{
    size_t total_heap = heap_caps_get_total_size(MALLOC_CAP_INTERNAL);
    size_t free_heap = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    size_t used_heap = total_heap - free_heap;

    SimpleShell::system(argc, argv);
    this->printf("Total Heap: %zu\n", total_heap);
    this->printf(" Free Heap: %zu\n", free_heap);
    this->printf(" Used Heap: %zu\n", used_heap);

    return 0;
}

int MeshRoofShell::reboot(int argc, char **argv)
{
    (void)(argc);
    (void)(argv);

    meshroof->sendDisconnect();
    esp_restart();
    for (;;);

    return 0;
}

int MeshRoofShell::wcfg(int argc, char **argv)
{
    int ret = 0;

    (void)(argc);
    (void)(argv);

    if (meshroof->sendWantConfig() != true) {
        this->printf("failed!\n");
        ret = -1;
    }

    return ret;
}

int MeshRoofShell::disc(int argc, char **argv)
{
   int ret = 0;

    (void)(argc);
    (void)(argv);

    if (meshroof->sendDisconnect() != true) {
        this->printf("failed!\n");
        ret = -1;
    }

    return ret;
}

int MeshRoofShell::hb(int argc, char **argv)
{
   int ret = 0;

    (void)(argc);
    (void)(argv);

    if (meshroof->sendHeartbeat() != true) {
        this->printf("failed!\n");
        ret = -1;
    }

    return ret;
}

int MeshRoofShell::unknown_command(int argc, char **argv)
{
    int ret = 0;

    if (strcmp(argv[0], "wcfg") == 0) {
        ret = this->wcfg(argc, argv);
    } else if (strcmp(argv[0], "disc") == 0) {
        ret = this->disc(argc, argv);
    } else if (strcmp(argv[0], "hb") == 0) {
        ret = this->hb(argc, argv);
    } else {
        this->printf("Unknown command '%s'!\n", argv[0]);
        ret = -1;
    }

    return ret;
}

/*
 * Local variables:
 * mode: C++
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
