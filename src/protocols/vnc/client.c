/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#include "config.h"

#include "client.h"
#include "user.h"
#include "vnc.h"

#ifdef ENABLE_COMMON_SSH
#include "guac_sftp.h"
#include "guac_ssh.h"
#include "sftp.h"
#endif

#ifdef ENABLE_PULSE
#include "pulse.h"
#endif

#include <guacamole/client.h>

#include <stdlib.h>
#include <string.h>

int guac_client_init(guac_client* client) {

    /* Set client args */
    client->args = GUAC_VNC_CLIENT_ARGS;

    /* Alloc client data */
    guac_vnc_client* vnc_client = calloc(1, sizeof(guac_vnc_client));
    client->data = vnc_client;

    /* Init clipboard */
    vnc_client->clipboard = guac_common_clipboard_alloc(GUAC_VNC_CLIPBOARD_MAX_LENGTH);

    /* Set handlers */
    client->join_handler = guac_vnc_user_join_handler;
    client->leave_handler = guac_vnc_user_leave_handler;
    client->free_handler = guac_vnc_client_free_handler;

    return 0;
}

int guac_vnc_client_free_handler(guac_client* client) {

    guac_vnc_client* vnc_client = (guac_vnc_client*) client->data;
    guac_vnc_settings* settings = vnc_client->settings;

    /* Clean up VNC client*/
    rfbClient* rfb_client = vnc_client->rfb_client;
    if (rfb_client != NULL) {

        /* Wait for client thread to finish */
        pthread_join(vnc_client->client_thread, NULL);

        /* Free memory not free'd by libvncclient's rfbClientCleanup() */
        if (rfb_client->frameBuffer != NULL) free(rfb_client->frameBuffer);
        if (rfb_client->raw_buffer != NULL) free(rfb_client->raw_buffer);
        if (rfb_client->rcSource != NULL) free(rfb_client->rcSource);

        /* Free VNC rfbClientData linked list (not free'd by rfbClientCleanup()) */
        while (rfb_client->clientData != NULL) {
            rfbClientData* next = rfb_client->clientData->next;
            free(rfb_client->clientData);
            rfb_client->clientData = next;
        }

        rfbClientCleanup(rfb_client);

    }

#ifdef ENABLE_COMMON_SSH
    /* Free SFTP filesystem, if loaded */
    if (vnc_client->sftp_filesystem)
        guac_common_ssh_destroy_sftp_filesystem(vnc_client->sftp_filesystem);

    /* Free SFTP session */
    if (vnc_client->sftp_session)
        guac_common_ssh_destroy_session(vnc_client->sftp_session);

    /* Free SFTP user */
    if (vnc_client->sftp_user)
        guac_common_ssh_destroy_user(vnc_client->sftp_user);

    guac_common_ssh_uninit();
#endif

    /* Free clipboard */
    if (vnc_client->clipboard != NULL)
        guac_common_clipboard_free(vnc_client->clipboard);

    /* Free display */
    if (vnc_client->display != NULL)
        guac_common_display_free(vnc_client->display);

    /* Free settings-dependend data */
    if (settings != NULL) {

#ifdef ENABLE_PULSE
        /* If audio enabled, stop streaming */
        if (settings->audio_enabled)
            guac_pa_stop_stream(client);
#endif

        /* Free parsed settings */
        guac_vnc_settings_free(settings);

    }

    /* Free generic data struct */
    free(client->data);

    return 0;
}

