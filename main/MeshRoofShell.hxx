/*
 * MeshRoofShell.hxx
 *
 * Copyright (C) 2025, Charles Chiou
 */

#ifndef MESHROOFSHELL_HXX
#define MESHROOFSHELL_HXX

#include <SimpleShell.hxx>

using namespace std;

class MeshRoofShell : public SimpleShell {

public:

    MeshRoofShell(shared_ptr<SimpleClient> client = NULL);
    ~MeshRoofShell();

protected:

    virtual int tx_write(const uint8_t *buf, size_t size);
    virtual int printf(const char *format, ...);
    virtual int rx_ready(void) const;
    virtual int rx_read(uint8_t *buf, size_t size);

    virtual int system(int argc, char **argv);
    virtual int reboot(int argc, char **argv);
    virtual int nvm(int argc, char **argv);
    virtual int wcfg(int argc, char **argv);
    virtual int disc(int argc, char **argv);
    virtual int hb(int argc, char **argv);
    virtual int wifi(int argc, char **argv);
    virtual int net(int argc, char **argv);
    virtual int ping(int argc, char **argv);
    virtual int unknown_command(int argc, char **argv);

};

#endif

/*
 * Local variables:
 * mode: C++
 * c-file-style: "BSD"
 * c-basic-offset: 4
 * tab-width: 4
 * indent-tabs-mode: nil
 * End:
 */
