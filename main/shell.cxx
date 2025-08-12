/*
 * shell.cxx
 *
 * Copyright (C) 2025, Charles Chiou
 */

#include <ctype.h>
#include <malloc.h>
#include <string.h>
#include <strings.h>
#include <esp_timer.h>
#include <esp_system.h>
#include <esp_heap_caps.h>
#include <meshroof.h>
#include <libmeshtastic.h>
#include <memory>
#include <MeshRoof.hxx>
#include "version.h"

#define shell_printf usb_printf

#define CMDLINE_SIZE 256

struct inproc {
    char cmdline[CMDLINE_SIZE];
    unsigned int i;
};

struct cmd_handler {
    const char *name;
    int (*f)(int argc, char **argv);
};

static struct inproc inproc;
extern shared_ptr<MeshRoof> meshroof;

static int version(int argc, char **argv)
{
    (void)(argc);
    (void)(argv);

    shell_printf("Version: %s\n", MYPROJECT_VERSION_STRING);
    shell_printf("Built: %s@%s %s\n",
                 MYPROJECT_WHOAMI, MYPROJECT_HOSTNAME, MYPROJECT_DATE);

    return 0;
}

static int system(int argc, char **argv)
{
    size_t total_heap = heap_caps_get_total_size(MALLOC_CAP_INTERNAL);
    size_t free_heap = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    size_t used_heap = total_heap - free_heap;
    unsigned int uptime, days, hour, min, sec;
    int64_t since_boot;

    (void)(argc);
    (void)(argv);

    since_boot = esp_timer_get_time();
    uptime = since_boot / 1000000;
    sec = (uptime % 60);
    min = (uptime / 60) % 60;
    hour = (uptime / 3600) % 24;
    days = (uptime) / 86400;
    if (days == 0) {
        shell_printf("   Up-time: %.2u:%.2u:%.2u\n",
                     hour, min, sec);
    } else {
        shell_printf("   Up-time: %ud %.2u:%.2u:%.2u\n",
                     days, hour, min, sec);
    }
    shell_printf("Total Heap: %zu\n", total_heap);
    shell_printf(" Free Heap: %zu\n", free_heap);
    shell_printf(" Used Heap: %zu\n", used_heap);

    return 0;
}

static int reboot(int argc, char **argv)
{
    (void)(argc);
    (void)(argv);

    meshroof->sendDisconnect();
    esp_restart();
    for (;;);

    return 0;
}

static int status(int argc, char **argv)
{
    int ret = 0;

    (void)(argc);
    (void)(argv);

    if (!meshroof->isConnected()) {
        shell_printf("Not connected\n");
    } else {
        unsigned int i;

        shell_printf("Me: %s %s\n",
                      meshroof->getDisplayName(meshroof->whoami()).c_str(),
                      meshroof->lookupLongName(meshroof->whoami()).c_str());

        shell_printf("Channels: %d\n",
                      meshroof->channels().size());
        for (map<uint8_t, meshtastic_Channel>::const_iterator it =
                 meshroof->channels().begin();
             it != meshroof->channels().end(); it++) {
            if (it->second.has_settings &&
                it->second.role != meshtastic_Channel_Role_DISABLED) {
                shell_printf("chan#%u: %s\n",
                              (unsigned int) it->second.index,
                              it->second.settings.name);
            }
        }

        shell_printf("Nodes: %d seen\n",
                      meshroof->nodeInfos().size());
        i = 0;
        for (map<uint32_t, meshtastic_NodeInfo>::const_iterator it =
                 meshroof->nodeInfos().begin();
             it != meshroof->nodeInfos().end(); it++, i++) {
            if ((i % 4) == 0) {
                shell_printf("  ");
            }
            shell_printf("%16s  ",
                          meshroof->getDisplayName(it->second.num).c_str());
            if ((i % 4) == 3) {
                shell_printf("\n");
            }
        }
        if ((i % 4) != 0) {
            shell_printf("\n");
        }

        map<uint32_t, meshtastic_DeviceMetrics>::const_iterator dev;
        dev = meshroof->deviceMetrics().find(meshroof->whoami());
        if (dev != meshroof->deviceMetrics().end()) {
            if (dev->second.has_channel_utilization) {
                shell_printf("channel_utilization: %.2f\n",
                              dev->second.channel_utilization);
            }
            if (dev->second.has_air_util_tx) {
                shell_printf("air_util_tx: %.2f\n",
                              dev->second.air_util_tx);
            }
        }

        map<uint32_t, meshtastic_EnvironmentMetrics>::const_iterator env;
        env = meshroof->environmentMetrics().find(meshroof->whoami());
        if (env != meshroof->environmentMetrics().end()) {
            if (env->second.has_temperature) {
                shell_printf("temperature: %.2f\n",
                              env->second.temperature);
            }
            if (env->second.has_relative_humidity) {
                shell_printf("relative_humidity: %.2f\n",
                              env->second.relative_humidity);
            }
            if (env->second.has_barometric_pressure) {
                shell_printf("barometric_pressure: %.2f\n",
                              env->second.barometric_pressure);
            }
        }
    }

    return ret;
}

static int want_config(int argc, char **argv)
{
    int ret = 0;

    (void)(argc);
    (void)(argv);

    if (meshroof->sendWantConfig() != true) {
        shell_printf("failed!\n");
        ret = -1;
    }

    return ret;
}

static int disconnect(int argc, char **argv)
{
   int ret = 0;

    (void)(argc);
    (void)(argv);

    if (meshroof->sendDisconnect() != true) {
        shell_printf("failed!\n");
        ret = -1;
    }

    return ret;
}

static int heartbeat(int argc, char **argv)
{
   int ret = 0;

    (void)(argc);
    (void)(argv);

    if (meshroof->sendHeartbeat() != true) {
        shell_printf("failed!\n", ret);
        ret = -1;
    }

    return ret;
}

static int direct_message(int argc, char **argv)
{
    int ret = 0;
    uint32_t dest = 0xffffffffU;
    string message;

    if (argc < 3) {
        shell_printf("Usage: %s [name] message\n", argv[0]);
        ret = -1;
        goto done;
    }

    dest = meshroof->getId(argv[1]);
    if ((dest == 0xffffffffU) || (dest == meshroof->whoami())) {
        shell_printf("name '%s' is invalid!\n", argv[1]);
        ret = -1;
        goto done;
    }

    for (int i = 2; i < argc; i++) {
        message += argv[i];
        message += " ";
    }

    if (meshroof->textMessage(dest, 0x0U, message) != true) {
        shell_printf("failed!\n");
        ret = -1;
        goto done;
    }

    ret = 0;

done:

    return ret;
}

static int channel_message(int argc, char **argv)
{
    int ret = 0;
    uint8_t channel = 0xffU;
    string message;

    if (argc < 3) {
        shell_printf("Usage: %s [chan] message\n", argv[0]);
        ret = -1;
        goto done;
    }

    channel = meshroof->getChannel(argv[1]);
    if (channel == 0xffU) {
        shell_printf("channel '%s' is invalid!\n", argv[1]);
        ret = -1;
        goto done;
    }

    for (int i = 2; i < argc; i++) {
        message += argv[i];
        message += " ";
    }

    if (meshroof->textMessage(0xffffffffU, channel, message) != true) {
        shell_printf("failed!\n");
        ret = -1;
        goto done;
    }

    ret = 0;

done:

    return ret;
}

static int authchan(int argc, char **argv)
{
    int ret = 0;
    bool result;

    if (argc == 1) {
        shell_printf("list of authchans:\n");
        for (unsigned int i = 0; i < meshroof->nvmAuthchans().size(); i++) {
            shell_printf("  %s\n", meshroof->nvmAuthchans()[i].name);
        }
    } else if ((argc == 3) && strcmp(argv[1], "add") == 0) {
        result = meshroof->addNvmAuthChannel(argv[2], *meshroof);
        if (result == false) {
            shell_printf("addNvmAuthChannel failed!\n");
            ret = -1;
            goto done;
        }
        result = meshroof->applyNvmToHomeChat();
        if (result == false) {
            shell_printf("applyNvmToHomeChat failed!\n");
            ret = -1;
            goto done;
        }
        result = meshroof->saveNvm();
        if (result == false) {
            shell_printf("saveNvm failed!\n");
            ret = -1;
            goto done;
        }
        shell_printf("ok\n");
    } else if ((argc == 3) && strcmp(argv[1], "del") == 0) {
        result = meshroof->delNvmAuthChannel(argv[2]);
        if (result == false) {
            shell_printf("delNvmAuthChannel failed!\n");
            ret = -1;
            goto done;
        }
        result = meshroof->applyNvmToHomeChat();
        if (result == false) {
            shell_printf("applyNvmToHomeChat failed!\n");
            ret = -1;
            goto done;
        }
        result = meshroof->saveNvm();
        if (result == false) {
            shell_printf("saveNvm failed!\n");
            ret = -1;
            goto done;
        }
        shell_printf("ok\n");
    } else {
        shell_printf("syntax error!\n");
        ret = -1;
        goto done;
    }

    ret = 0;

done:

    return ret;
}

static int admin(int argc, char **argv)
{
    int ret = 0;
    bool result;

    if (argc == 1) {
        unsigned int i;
        shell_printf("list of admins:\n");
        for (i = 0; i < meshroof->nvmAdmins().size(); i++) {
            uint32_t node_num = meshroof->nvmAdmins()[i].node_num;
            if ((i % 4) == 0) {
                shell_printf("  ");
            }
            shell_printf("%16s  ",
                           meshroof->getDisplayName(node_num).c_str());
            if ((i % 4) == 3) {
                shell_printf("\n");
            }
        }
        if ((i % 4) != 0) {
            shell_printf("\n");
        }
    } else if ((argc == 3) && strcmp(argv[1], "add") == 0) {
        result = meshroof->addNvmAdmin(argv[2], *meshroof);
        if (result == false) {
            shell_printf("addNvmAdmin failed!\n");
            ret = -1;
            goto done;
        }
        result = meshroof->applyNvmToHomeChat();
        if (result == false) {
            shell_printf("applyNvmToHomeChat failed!\n");
            ret = -1;
            goto done;
        }
        result = meshroof->saveNvm();
        if (result == false) {
            shell_printf("saveNvm failed!\n");
            ret = -1;
            goto done;
        }
        shell_printf("ok\n");
    } else if ((argc == 3) && strcmp(argv[1], "del") == 0) {
        result = meshroof->delNvmAdmin(argv[2], *meshroof);
        if (result == false) {
            shell_printf("delNvmAdmin failed!\n");
            ret = -1;
            goto done;
        }
        result = meshroof->applyNvmToHomeChat();
        if (result == false) {
            shell_printf("applyNvmToHomeChat failed!\n");
            ret = -1;
            goto done;
        }
        result = meshroof->saveNvm();
        if (result == false) {
            shell_printf("saveNvm failed!\n");
            ret = -1;
            goto done;
        }
        shell_printf("ok\n");
    } else {
        shell_printf("syntax error!\n");
        ret = -1;
        goto done;
    }

    ret = 0;

done:

    return ret;
}

static int mate(int argc, char **argv)
{
    int ret = 0;
    bool result;

    if (argc == 1) {
        unsigned int i;
        shell_printf("list of mates:\n");
        for (i = 0; i < meshroof->nvmMates().size(); i++) {
            uint32_t node_num = meshroof->nvmMates()[i].node_num;
            if ((i % 4) == 0) {
                shell_printf("  ");
            }
            shell_printf("%16s  ",
                           meshroof->getDisplayName(node_num).c_str());
            if ((i % 4) == 3) {
                shell_printf("\n");
            }
        }
        if ((i % 4) != 0) {
            shell_printf("\n");
        }
    } else if ((argc == 3) && strcmp(argv[1], "add") == 0) {
        result = meshroof->addNvmMate(argv[2], *meshroof);
        if (result == false) {
            shell_printf("addNvmMate failed!\n");
            ret = -1;
            goto done;
        }
        result = meshroof->applyNvmToHomeChat();
        if (result == false) {
            shell_printf("applyNvmToHomeChat failed!\n");
            ret = -1;
            goto done;
        }
        result = meshroof->saveNvm();
        if (result == false) {
            shell_printf("saveNvm failed!\n");
            ret = -1;
            goto done;
        }
        shell_printf("ok\n");
    } else if ((argc == 3) && strcmp(argv[1], "del") == 0) {
        result = meshroof->delNvmMate(argv[2], *meshroof);
        if (result == false) {
            shell_printf("delNvmMate failed!\n");
            ret = -1;
            goto done;
        }
        result = meshroof->applyNvmToHomeChat();
        if (result == false) {
            shell_printf("applyNvmToHomeChat failed!\n");
            ret = -1;
            goto done;
        }
        result = meshroof->saveNvm();
        if (result == false) {
            shell_printf("saveNvm failed!\n");
            ret = -1;
            goto done;
        }
        shell_printf("ok\n");
    } else {
        shell_printf("syntax error!\n");
        ret = -1;
        goto done;
    }

    ret = 0;

done:

    return ret;
}

static int nvm(int argc, char **argv)
{
    if (argc != 1) {
        shell_printf("syntax error!\n");
        return -1;
    }

    authchan(argc, argv);
    admin(argc, argv);
    mate(argc, argv);

    return 0;
}

static int help(int argc, char **argv);

static struct cmd_handler cmd_handlers[] = {
    { "help", help, },
    { "version", version, },
    { "system", system, },
    { "reboot", reboot, },
    { "status", status, },
    { "want_config", want_config, },
    { "disconnect", disconnect, },
    { "heartbeat", heartbeat, },
    { "dm", direct_message },
    { "cm", channel_message, },
    { "authchan", authchan, },
    { "admin", admin, },
    { "mate", mate, },
    { "nvm", nvm, },
    { NULL, NULL, },
};

static int help(int argc, char **argv)
{
    int i;

    (void)(argc);
    (void)(argv);

    shell_printf("Available commands:\n");

    for (i = 0; cmd_handlers[i].name && cmd_handlers[i].f; i++) {
        if ((i % 4) == 0) {
            shell_printf("\t");
        }

        shell_printf("%s\t", cmd_handlers[i].name);

        if ((i % 4) == 3) {
            shell_printf("\n");
        }
    }

    if ((i % 4) != 0) {
        shell_printf("\n");
    }

    return 0;
}

static void execute_cmdline(char *cmdline)
{
    int argc = 0;
    char *argv[32];

    bzero(argv, sizeof(argv));

    // Tokenize cmdline into argv[]
    while (*cmdline != '\0' && (argc < 32)) {
        while ((*cmdline != '\0') && isspace((int) *cmdline)) {
            cmdline++;
        }

        if (*cmdline == '\0') {
            break;
        }

        argv[argc] = cmdline;
        argc++;

        while ((*cmdline != '\0') && !isspace((int) *cmdline)) {
            cmdline++;
        }

        if (*cmdline == '\0') {
            break;
        }

        *cmdline = '\0';
        cmdline++;
    }

    if (argc == 0) {
        goto done;
    }

    for (int i = 0; cmd_handlers[i].name && cmd_handlers[i].f; i++) {
        if (strcmp(cmd_handlers[i].name, argv[0]) == 0) {
            cmd_handlers[i].f(argc, argv);
            goto done;
        }
    }

    shell_printf("Unknown command: '%s'!\n", argv[0]);

done:

    return;
}

void shell_init(void)
{
    bzero(&inproc, sizeof(inproc));

    shell_printf("> ");
}

int shell_process(void)
{
    int ret = 0;
    int rx;
    char c;

    while (usb_rx_ready() > 0) {
        rx = usb_rx_read((uint8_t *) &c, 1);
        if (rx == 0) {
            break;
        }

        ret += rx;

        if (c == '\r') {
            inproc.cmdline[inproc.i] = '\0';
            shell_printf("\n");
            execute_cmdline(inproc.cmdline);
            shell_printf("> ");
            inproc.i = 0;
            inproc.cmdline[0] = '\0';
        } else if ((c == '\x7f') || (c == '\x08')) {
            if (inproc.i > 0) {
                shell_printf("\b \b");
                inproc.i--;
            }
        } else if (c == '\x03') {
            shell_printf("^C\n> ");
            inproc.i = 0;
        } else if ((c != '\n') && isprint(c)) {
            if (inproc.i < (CMDLINE_SIZE - 1)) {
                shell_printf("%c", c);
                inproc.cmdline[inproc.i] = c;
                inproc.i++;
            }
        }
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
