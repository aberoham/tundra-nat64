/*
Copyright (c) 2024 Vít Labuda. All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the
following conditions are met:
 1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following
    disclaimer.
 2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the
    following disclaimer in the documentation and/or other materials provided with the distribution.
 3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote
    products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include"t64_tundra.h"
#include"t64_opmode_translate.h"

#include"t64_utils.h"
#include"t64_log.h"
#include"t64_init_io.h"
#include"t64_signal.h"
#include"t64_xlat.h"
#include"t64_conf_file.h"
#include"t64_conf_cmdline.h"


static t64ts_tundra__xlat_thread_context *_t64fa_opmode_translate__initialize_xlat_thread_contexts(const t64ts_tundra__conf_cmdline *cmdline_configuration, const t64ts_tundra__conf_file *file_configuration);
static t64ts_tundra__external_addr_xlat_state *_t64fa_opmode_translate__initialize_external_addr_xlat_state_struct(const t64ts_tundra__conf_file *file_configuration, char **addressing_external_next_fds_string_ptr);
static void _t64f_opmode_translate__free_xlat_thread_contexts(const t64ts_tundra__conf_file *file_configuration, t64ts_tundra__xlat_thread_context *thread_contexts);
static void _t64f_opmode_translate__free_external_addr_xlat_state_struct(t64ts_tundra__external_addr_xlat_state *external_addr_xlat_state);
static void _t64f_opmode_translate__daemonize(const t64ts_tundra__conf_file *file_configuration);
static void _t64f_opmode_translate__start_xlat_threads(const t64ts_tundra__conf_file *file_configuration, t64ts_tundra__xlat_thread_context *thread_contexts);
static void _t64f_opmode_translate__print_information_about_translation_start(const t64ts_tundra__conf_file *file_configuration);
static void _t64f_opmode_translate__monitor_xlat_threads(const t64ts_tundra__conf_file *file_configuration, t64ts_tundra__xlat_thread_context *thread_contexts);
static void _t64f_opmode_translate__terminate_xlat_threads(const t64ts_tundra__conf_file *file_configuration, t64ts_tundra__xlat_thread_context *thread_contexts);


void t64f_opmode_translate__run(const t64ts_tundra__conf_cmdline *cmdline_configuration, const t64ts_tundra__conf_file *file_configuration) {
    if(file_configuration->program_translator_threads < 1 || file_configuration->program_translator_threads > T64C_TUNDRA__MAX_TRANSLATOR_THREADS)
        t64f_log__crash_invalid_internal_state("Invalid count of translator threads");

    t64f_log__info("%s", T64C_TUNDRA__PROGRAM_INFO_STRING);

    t64ts_tundra__xlat_thread_context *thread_contexts = _t64fa_opmode_translate__initialize_xlat_thread_contexts(cmdline_configuration, file_configuration);
    _t64f_opmode_translate__daemonize(file_configuration);
    _t64f_opmode_translate__start_xlat_threads(file_configuration, thread_contexts);
    _t64f_opmode_translate__print_information_about_translation_start(file_configuration);

    _t64f_opmode_translate__monitor_xlat_threads(file_configuration, thread_contexts);

    _t64f_opmode_translate__terminate_xlat_threads(file_configuration, thread_contexts);
    _t64f_opmode_translate__free_xlat_thread_contexts(file_configuration, thread_contexts);

    t64f_log__info("Tundra will now terminate.");
}

static t64ts_tundra__xlat_thread_context *_t64fa_opmode_translate__initialize_xlat_thread_contexts(const t64ts_tundra__conf_cmdline *cmdline_configuration, const t64ts_tundra__conf_file *file_configuration) {
    t64ts_tundra__xlat_thread_context *thread_contexts = t64fa_utils__allocate_zeroed_out_memory(file_configuration->program_translator_threads, sizeof(t64ts_tundra__xlat_thread_context));

    if(file_configuration->io_mode == T64TE_TUNDRA__IO_MODE_INHERITED_FDS && cmdline_configuration->io_inherited_fds == NULL)
        t64f_log__crash(false, "Even though the program is in the '%s' I/O mode, the '-%c' / '--%s' command-line option is missing!", T64C_CONF_FILE__IO_MODE_INHERITED_FDS, T64C_CONF_CMDLINE__SHORTOPT_IO_INHERITED_FDS, T64C_CONF_CMDLINE__LONGOPT_IO_INHERITED_FDS);
    if(file_configuration->addressing_mode == T64TE_TUNDRA__ADDRESSING_MODE_EXTERNAL && file_configuration->addressing_external_transport == T64TE_TUNDRA__ADDRESSING_EXTERNAL_TRANSPORT_INHERITED_FDS && cmdline_configuration->addressing_external_inherited_fds == NULL)
        t64f_log__crash(false, "Even though the program is configured to use the '%s' transport of the '%s' addressing mode, the '-%c' / '--%s' command-line option is missing!", T64C_CONF_FILE__ADDRESSING_EXTERNAL_TRANSPORT_INHERITED_FDS, T64C_CONF_FILE__ADDRESSING_MODE_EXTERNAL, T64C_CONF_CMDLINE__SHORTOPT_ADDRESSING_EXTERNAL_INHERITED_FDS, T64C_CONF_CMDLINE__LONGOPT_ADDRESSING_EXTERNAL_INHERITED_FDS);

    char *io_next_fds_string_ptr = cmdline_configuration->io_inherited_fds;
    char *addressing_external_next_fds_string_ptr = cmdline_configuration->addressing_external_inherited_fds;
    int single_queue_tun_fd = -1;

    for(size_t i = 0; i < file_configuration->program_translator_threads; i++) {
        // context[i].thread stays uninitialized (it is initialized in _t64f_opmode_translate_start_xlat_threads())
        thread_contexts[i].thread_id = (i + 1); // Thread ID 0 is reserved for the main thread
        thread_contexts[i].in_packet_buffer = t64fa_utils__allocate_zeroed_out_memory(T64C_TUNDRA__MAX_PACKET_SIZE + 1, sizeof(uint8_t));
        thread_contexts[i].in_packet_size = 0;
        thread_contexts[i].configuration = file_configuration;
        thread_contexts[i].joined = false;

        if(getrandom(&thread_contexts[i].fragment_identifier_ipv6, 4, 0) != 4 || getrandom(&thread_contexts[i].fragment_identifier_ipv4, 2, 0) != 2)
            t64f_log__crash(false, "Failed to generate fragment identifiers using the getrandom() system call!");

        thread_contexts[i].external_addr_xlat_state = (
            (file_configuration->addressing_mode == T64TE_TUNDRA__ADDRESSING_MODE_EXTERNAL) ?
            _t64fa_opmode_translate__initialize_external_addr_xlat_state_struct(file_configuration, &addressing_external_next_fds_string_ptr) :
            NULL
        );

        switch(file_configuration->io_mode) {
            case T64TE_TUNDRA__IO_MODE_INHERITED_FDS:
                io_next_fds_string_ptr = t64f_init_io__get_fd_pair_from_inherited_fds_string(&thread_contexts[i].packet_read_fd, &thread_contexts[i].packet_write_fd, io_next_fds_string_ptr, T64C_CONF_CMDLINE__SHORTOPT_IO_INHERITED_FDS, T64C_CONF_CMDLINE__LONGOPT_IO_INHERITED_FDS);
                break;

            case T64TE_TUNDRA__IO_MODE_TUN:
                if(file_configuration->io_tun_multi_queue) {
                    thread_contexts[i].packet_read_fd = t64f_init_io__open_tun_interface(file_configuration);
                    thread_contexts[i].packet_write_fd = thread_contexts[i].packet_read_fd;
                } else {
                    if(single_queue_tun_fd < 0)
                        single_queue_tun_fd = t64f_init_io__open_tun_interface(file_configuration);

                    thread_contexts[i].packet_read_fd = single_queue_tun_fd;
                    thread_contexts[i].packet_write_fd = single_queue_tun_fd;
                }
                break;

            default:
                t64f_log__crash_invalid_internal_state("Invalid I/O mode");
        }
    }

    return thread_contexts;
}

static t64ts_tundra__external_addr_xlat_state *_t64fa_opmode_translate__initialize_external_addr_xlat_state_struct(const t64ts_tundra__conf_file *file_configuration, char **addressing_external_next_fds_string_ptr) {
    t64ts_tundra__external_addr_xlat_state *external_addr_xlat_state = t64fa_utils__allocate_zeroed_out_memory(1, sizeof(t64ts_tundra__external_addr_xlat_state));

    if(file_configuration->addressing_external_cache_size_main_addresses > 0) {
        // It is absolutely crucial that the cache memory is zeroed out!
        external_addr_xlat_state->address_cache_4to6_main_packet = t64fa_utils__allocate_zeroed_out_memory(file_configuration->addressing_external_cache_size_main_addresses, sizeof(t64ts_tundra__external_addr_xlat_cache_entry));
        external_addr_xlat_state->address_cache_6to4_main_packet = t64fa_utils__allocate_zeroed_out_memory(file_configuration->addressing_external_cache_size_main_addresses, sizeof(t64ts_tundra__external_addr_xlat_cache_entry));
    } else {
        external_addr_xlat_state->address_cache_4to6_main_packet = NULL;
        external_addr_xlat_state->address_cache_6to4_main_packet = NULL;
    }

    if(file_configuration->addressing_external_cache_size_icmp_error_addresses > 0) {
        // It is absolutely crucial that the cache memory is zeroed out!
        external_addr_xlat_state->address_cache_4to6_icmp_error_packet = t64fa_utils__allocate_zeroed_out_memory(file_configuration->addressing_external_cache_size_icmp_error_addresses, sizeof(t64ts_tundra__external_addr_xlat_cache_entry));
        external_addr_xlat_state->address_cache_6to4_icmp_error_packet = t64fa_utils__allocate_zeroed_out_memory(file_configuration->addressing_external_cache_size_icmp_error_addresses, sizeof(t64ts_tundra__external_addr_xlat_cache_entry));
    } else {
        external_addr_xlat_state->address_cache_4to6_icmp_error_packet = NULL;
        external_addr_xlat_state->address_cache_6to4_icmp_error_packet = NULL;
    }

    if(file_configuration->addressing_external_transport == T64TE_TUNDRA__ADDRESSING_EXTERNAL_TRANSPORT_INHERITED_FDS) {
        *addressing_external_next_fds_string_ptr = t64f_init_io__get_fd_pair_from_inherited_fds_string(&external_addr_xlat_state->read_fd, &external_addr_xlat_state->write_fd, *addressing_external_next_fds_string_ptr, T64C_CONF_CMDLINE__SHORTOPT_ADDRESSING_EXTERNAL_INHERITED_FDS, T64C_CONF_CMDLINE__LONGOPT_ADDRESSING_EXTERNAL_INHERITED_FDS);
    } else {
        external_addr_xlat_state->read_fd = external_addr_xlat_state->write_fd = -1;
    }

    if(getrandom(&external_addr_xlat_state->message_identifier, 4, 0) != 4)
        t64f_log__crash(false, "Failed to generate a message identifier for external address translation using the getrandom() system call!");

    return external_addr_xlat_state;
}

// Closes 'packet_read_fd' and 'packet_write_fd', but not 'termination_pipe_read_fd'!
static void _t64f_opmode_translate__free_xlat_thread_contexts(const t64ts_tundra__conf_file *file_configuration, t64ts_tundra__xlat_thread_context *thread_contexts) {
    for(size_t i = 0; i < file_configuration->program_translator_threads; i++) {
        t64f_utils__free_memory(thread_contexts[i].in_packet_buffer);

        if(thread_contexts[i].external_addr_xlat_state != NULL)
            _t64f_opmode_translate__free_external_addr_xlat_state_struct(thread_contexts[i].external_addr_xlat_state);

        t64f_init_io__close_fd(thread_contexts[i].packet_read_fd, true);
        t64f_init_io__close_fd(thread_contexts[i].packet_write_fd, true);
    }

    t64f_utils__free_memory(thread_contexts);
}

static void _t64f_opmode_translate__free_external_addr_xlat_state_struct(t64ts_tundra__external_addr_xlat_state *external_addr_xlat_state) {
    if(external_addr_xlat_state->address_cache_4to6_main_packet != NULL)
        t64f_utils__free_memory(external_addr_xlat_state->address_cache_4to6_main_packet);
    if(external_addr_xlat_state->address_cache_4to6_icmp_error_packet != NULL)
        t64f_utils__free_memory(external_addr_xlat_state->address_cache_4to6_icmp_error_packet);
    if(external_addr_xlat_state->address_cache_6to4_main_packet != NULL)
        t64f_utils__free_memory(external_addr_xlat_state->address_cache_6to4_main_packet);
    if(external_addr_xlat_state->address_cache_6to4_icmp_error_packet != NULL)
        t64f_utils__free_memory(external_addr_xlat_state->address_cache_6to4_icmp_error_packet);

    t64f_init_io__close_fd(external_addr_xlat_state->read_fd, true);
    t64f_init_io__close_fd(external_addr_xlat_state->write_fd, true);

    t64f_utils__free_memory(external_addr_xlat_state);
}

static void _t64f_opmode_translate__daemonize(const t64ts_tundra__conf_file *file_configuration) {
    // --- chdir() ---
    if(chdir(T64C_TUNDRA__WORKING_DIRECTORY) < 0)
        t64f_log__crash(true, "Failed to change the program's working directory (the chdir() call failed)!");

    // --- setgroups() & setgid() ---
    if(file_configuration->program_privilege_drop_group_perform) {
        if(setgroups(1, &file_configuration->program_privilege_drop_group_gid) < 0)
            t64f_log__crash(true, "Failed to drop the program's group privileges to GID %"PRIdMAX" (the setgroups() call failed)!", (intmax_t) file_configuration->program_privilege_drop_group_gid);

        if(setgid(file_configuration->program_privilege_drop_group_gid) < 0)
            t64f_log__crash(true, "Failed to drop the program's group privileges to GID %"PRIdMAX" (the setgid() call failed)!", (intmax_t) file_configuration->program_privilege_drop_group_gid);
    }

    // --- setuid() ---
    if(file_configuration->program_privilege_drop_user_perform && setuid(file_configuration->program_privilege_drop_user_uid) < 0)
        t64f_log__crash(true, "Fail to drop the program's user privileges to UID %"PRIdMAX" (the setuid() call failed)!", (intmax_t) file_configuration->program_privilege_drop_user_uid);
}

static void _t64f_opmode_translate__start_xlat_threads(const t64ts_tundra__conf_file *file_configuration, t64ts_tundra__xlat_thread_context *thread_contexts) {
    for(size_t i = 0; i < file_configuration->program_translator_threads; i++) {
        const int pthread_errno = pthread_create(&thread_contexts[i].thread, NULL, t64f_xlat__thread_run, thread_contexts + i);
        if(pthread_errno != 0) {
            errno = pthread_errno;
            t64f_log__crash(true, "Failed to create a new translator thread!");
        }
    }
}

static void _t64f_opmode_translate__print_information_about_translation_start(const t64ts_tundra__conf_file *file_configuration) {
    const char *addressing_mode_string;
    switch(file_configuration->addressing_mode) {
        case T64TE_TUNDRA__ADDRESSING_MODE_NAT64: addressing_mode_string = "NAT64"; break;
        case T64TE_TUNDRA__ADDRESSING_MODE_CLAT: addressing_mode_string = "CLAT"; break;
        case T64TE_TUNDRA__ADDRESSING_MODE_SIIT: addressing_mode_string = "SIIT"; break;
        case T64TE_TUNDRA__ADDRESSING_MODE_EXTERNAL: addressing_mode_string = "<external>"; break;
        default: t64f_log__crash_invalid_internal_state("Invalid addressing mode");
    }

    switch(file_configuration->io_mode) {
        case T64TE_TUNDRA__IO_MODE_INHERITED_FDS:
            t64f_log__info("%zu threads are now performing %s translation of packets on command-line-provided file descriptors...", file_configuration->program_translator_threads, addressing_mode_string);
            break;

        case T64TE_TUNDRA__IO_MODE_TUN:
            t64f_log__info("%zu threads are now performing %s translation of packets on TUN interface '%s'...", file_configuration->program_translator_threads, addressing_mode_string, file_configuration->io_tun_interface_name);
            break;

        default:
            t64f_log__crash_invalid_internal_state("Invalid I/O mode");
    }
}

static void _t64f_opmode_translate__monitor_xlat_threads(const t64ts_tundra__conf_file *file_configuration, t64ts_tundra__xlat_thread_context *thread_contexts) {
    while(t64f_signal__should_this_thread_continue_running()) {
        for(size_t i = 0; i < file_configuration->program_translator_threads; i++) {
            if(pthread_tryjoin_np(thread_contexts[i].thread, NULL) != EBUSY)
                t64f_log__crash(false, "A translator thread has terminated unexpectedly!");
        }

        usleep(T64C_TUNDRA__TRANSLATOR_THREAD_MONITOR_INTERVAL_MICROSECONDS);
    }
}

static void _t64f_opmode_translate__terminate_xlat_threads(const t64ts_tundra__conf_file *file_configuration, t64ts_tundra__xlat_thread_context *thread_contexts) {
    // Even though it is extremely unlikely, threads may not terminate on first signal (due to a race condition - when
    //  the signal is delivered between t64f_signal__should_this_thread_continue_running() and a blocking system call);
    //  therefore, the termination is performed within an "infinite" loop.
    for(;;) {
        bool are_there_running_threads = false;

        for(size_t i = 0; i < file_configuration->program_translator_threads; i++) {
            if(thread_contexts[i].joined)
                continue;

            switch(pthread_tryjoin_np(thread_contexts[i].thread, NULL)) {
                case 0:
                    {
                        thread_contexts[i].joined = true;
                    }
                    break;

                case EBUSY:
                    {
                        are_there_running_threads = true;

                        // There is a possibility of a race condition occurring (when the thread terminates between
                        //  pthread_tryjoin_np() and pthread_kill()), but it is ignored, because the chance of it
                        //  occurring is extremely tiny, and there seems to be no easy (!) and performance-friendly way
                        //  of implementing the termination process 100% atomically.
                        if(pthread_kill(thread_contexts[i].thread, T64C_SIGNAL__TRANSLATOR_THREAD_TERMINATION_SIGNAL) != 0)
                            t64f_log__crash(false, "Failed to inform a translator thread that it should terminate (using a signal)!");
                    }
                    break;

                default:
                    t64f_log__crash(false, "Failed to join a translator thread!");
            }
        }

        if(are_there_running_threads)
            usleep(T64C_TUNDRA__TRANSLATOR_THREAD_TERMINATION_INTERVAL_MICROSECONDS);
        else
            break;
    }
}
