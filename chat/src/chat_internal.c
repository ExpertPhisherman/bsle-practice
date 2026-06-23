/** @file chat_internal.c
 *
 * @brief Chat server internal types and helpers
 *
 * @par
 *
 */

#include "chat_internal.h"

extern uint16_t const g_default_lport;
extern uint32_t const g_max_packet_size;
extern uint32_t const g_chunk_size;
extern size_t const   g_creds_capacity;
extern size_t const   g_admins_capacity;
extern size_t const   g_session_store_capacity;
extern size_t const   g_pm_reqs_capacity;
extern size_t const   g_file_reqs_capacity;
extern uint16_t const g_username_size_min;
extern uint16_t const g_username_size_max;
extern uint16_t const g_password_size_min;
extern uint16_t const g_password_size_max;

room_t *
room_create (char const * p_name, uint16_t name_size)
{
    status_t status = STATUS_SUCCESS;

    room_t  * p_room      = NULL;
    sll_t   * p_sessions  = NULL;
    ht_t    * p_pm_reqs   = NULL;
    ht_t    * p_file_reqs = NULL;
    uint8_t * p_plus_chr  = NULL;

    p_room = calloc(1u, sizeof(*p_room));
    if (NULL == p_room)
    {
        fprintf(stderr, "calloc failed in room_create\n");
        status = STATUS_ALLOC_FAILURE;
        goto cleanup;
    }

    p_room->name_size = name_size;

    p_room->p_name = calloc(name_size, sizeof(*(p_room->p_name)));
    if (NULL == p_room->p_name)
    {
        fprintf(stderr, "calloc failed in room_create\n");
        status = STATUS_ALLOC_FAILURE;
        goto cleanup;
    }

    memcpy(p_room->p_name, p_name, name_size);

    p_sessions = sll_create();
    if (NULL == p_sessions)
    {
        status = STATUS_ALLOC_FAILURE;
        goto cleanup;
    }
    p_room->p_sessions = p_sessions;

    p_pm_reqs = ht_create(g_pm_reqs_capacity);
    if (NULL == p_pm_reqs)
    {
        status = STATUS_ALLOC_FAILURE;
        goto cleanup;
    }
    p_room->p_pm_reqs = p_pm_reqs;

    p_file_reqs = ht_create(g_file_reqs_capacity);
    if (NULL == p_file_reqs)
    {
        status = STATUS_ALLOC_FAILURE;
        goto cleanup;
    }
    p_room->p_file_reqs = p_file_reqs;

    p_room->b_private  = false;
    p_room->p_user1    = NULL;
    p_room->user1_size = 0u;
    p_room->p_user2    = NULL;
    p_room->user2_size = 0u;

    // Check if '+' character is in room name
    p_plus_chr = memchr(p_room->p_name, '+', name_size);

    if (NULL != p_plus_chr)
    {
        p_room->b_private  = true;
        p_room->p_user1    = p_room->p_name;
        p_room->user1_size = p_plus_chr - p_room->p_user1;
        p_room->p_user2    = p_plus_chr + 1u;
        p_room->user2_size = p_room->p_name + name_size - p_room->p_user2;
    }

cleanup:
    if (STATUS_SUCCESS != status)
    {
        room_destroy(p_room);
        p_room = NULL;
    }

    return p_room;
}

void
room_destroy (void * p_data)
{
    if (NULL == p_data)
    {
        goto cleanup;
    }

    room_t * p_room = p_data;

    free(p_room->p_name);
    p_room->p_name = NULL;

    p_room->name_size = 0u;

    sll_destroy(p_room->p_sessions);
    p_room->p_sessions = NULL;

    ht_destroy(p_room->p_pm_reqs);
    p_room->p_pm_reqs = NULL;

    ht_destroy(p_room->p_file_reqs);
    p_room->p_file_reqs = NULL;

    p_room->b_private  = false;
    p_room->p_user1    = NULL;
    p_room->user1_size = 0u;
    p_room->p_user2    = NULL;
    p_room->user2_size = 0u;

    free(p_room);
    p_room = NULL;

cleanup:
    return;
}

uint32_t
user_login (session_t * p_session, appdata_t * p_appdata)
{
    uint32_t session_id = 0u;

    server_t * p_server      = NULL;
    ht_t     * p_cred_store  = NULL;
    item_t   * p_item        = NULL;
    uint16_t   username_size = 0u;
    uint16_t   password_size = 0u;
    uint8_t  * p_username    = NULL;
    uint8_t  * p_password    = NULL;

    if (
        (NULL == p_session) ||
        (NULL == p_appdata) ||
        (NULL == p_appdata->p_cred_store)
    )
    {
        goto cleanup;
    }

    p_server      = p_session->p_server;
    p_cred_store  = p_appdata->p_cred_store;
    username_size = p_session->username_size;
    password_size = p_session->password_size;
    p_username    = p_session->p_username;
    p_password    = p_session->p_password;

    // Authenticate login against hash table
    p_item = ht_get(p_cred_store, p_username, username_size);

    if (NULL != p_item)
    {
        // NOTE: User already exists

        // Check if password doesn't match
        if (!(
            (p_item->value_size == password_size) &&
            (0 == memcmp(p_item->p_value, p_password, password_size))
        ))
        {
            // NOTE: Incorrect password

            if (p_server->b_verbose)
            {
                printf(
                    "Incorrect password for user: %.*s\n",
                    username_size,
                    p_username
                );
            }

            goto cleanup;
        }

        if (p_server->b_verbose)
        {
            printf(
                "Successful login to user: %.*s\n",
                username_size,
                p_username
            );
        }
    }
    else
    {
        // NOTE: User doesn't exist

        // Create new user
        ht_set(
            p_cred_store,
            p_username,
            username_size,
            p_password,
            password_size
        );

        if (p_server->b_verbose)
        {
            printf(
                "Created new user: %.*s\n",
                username_size,
                p_username
            );
        }
    }

    session_id = (p_appdata->next_session_id)++;

cleanup:
    return session_id;
}

status_t
user_logout (session_t * p_session, appdata_t * p_appdata)
{
    status_t status = STATUS_SUCCESS;

    if (
        (NULL == p_session) ||
        (NULL == p_session->p_username) ||
        (NULL == p_session->p_password) ||
        (NULL == p_appdata)
    )
    {
        status = STATUS_NULL_ARG;
        goto cleanup;
    }

    user_leave(p_session, p_appdata);

    if (
        (NULL != p_appdata->p_session_store) &&
        (0u != p_session->username_size)
    )
    {
        ht_del(
            p_appdata->p_session_store,
            p_session->p_username,
            p_session->username_size
        );
    }

    memset(p_session->p_username, 0, g_username_size_max);
    memset(p_session->p_password, 0, g_password_size_max);

    p_session->username_size = 0u;
    p_session->password_size = 0u;
    p_session->session_id    = 0u;

cleanup:
    return status;
}

status_t
user_join (session_t * p_session, appdata_t * p_appdata)
{
    status_t status = STATUS_SUCCESS;

    room_t * p_room = NULL;

    if (
        (NULL == p_session) ||
        (NULL == p_session->p_username) ||
        (NULL == p_session->p_password) ||
        (NULL == p_appdata)
    )
    {
        status = STATUS_NULL_ARG;
        goto cleanup;
    }

    p_room = p_session->p_room;

    // Check if current user session is allowed to enter private room
    if (p_room->b_private)
    {
        if (!((
            (p_session->username_size == p_room->user1_size) &&
            (0 == memcmp(
                p_session->p_username,
                p_room->p_user1,
                p_session->username_size
            ))
            ) ||
            ((p_session->username_size == p_room->user2_size) &&
            (0 == memcmp(
                p_session->p_username,
                p_room->p_user2,
                p_session->username_size
            ))
        )))
        {
            status = STATUS_FAILURE;
            goto cleanup;
        }
    }

    if (NULL != session_by_username(
        p_room,
        p_session->p_username,
        p_session->username_size
    ))
    {
        // NOTE: User is already in room
        p_session->p_room = NULL;
        goto cleanup;
    }

    // Append user session to room's sessions SLL
    sll_append(p_room->p_sessions, &p_session, sizeof(p_session));

    // Set empty item in user's requests
    ht_set(
        p_room->p_pm_reqs,
        p_session->p_username,
        p_session->username_size,
        "",
        0u
    );

    ht_set(
        p_room->p_file_reqs,
        p_session->p_username,
        p_session->username_size,
        "",
        0u
    );

cleanup:
    return status;
}

status_t
user_leave (session_t * p_session, appdata_t * p_appdata)
{
    status_t status = STATUS_SUCCESS;

    room_t * p_room = NULL;

    if (
        (NULL == p_session) ||
        (NULL == p_session->p_room) ||
        (NULL == p_appdata) ||
        (NULL == p_appdata->p_room_store)
    )
    {
        status = STATUS_NULL_ARG;
        goto cleanup;
    }

    p_room = p_session->p_room;

    // Remove user session from room's sessions SLL
    sll_remove(p_room->p_sessions, &p_session, sizeof(p_session));

    // Delete item from user's requests
    ht_del(
        p_room->p_pm_reqs,
        p_session->p_username,
        p_session->username_size
    );

    ht_del(
        p_room->p_file_reqs,
        p_session->p_username,
        p_session->username_size
    );

    p_session->p_room = NULL;

cleanup:
    return status;
}

session_t *
session_by_username (
    room_t   * p_room,
    uint8_t  * p_username,
    uint16_t   username_size
)
{
    node_t    * p_curr    = NULL;
    session_t * p_session = NULL;

    if (
        (NULL == p_room) ||
        (NULL == p_room->p_sessions) ||
        (NULL == p_username)
    )
    {
        goto cleanup;
    }

    p_curr = p_room->p_sessions->p_head;
    while (NULL != p_curr)
    {
        p_session = *(session_t **)(p_curr->p_data);
        if (
            (p_session->username_size == username_size) &&
            (0 == memcmp(p_session->p_username, p_username, username_size))
        )
        {
            break;
        }

        p_session = NULL;
        p_curr    = p_curr->p_next;
    }

cleanup:
    return p_session;
}

bool
user_creds_valid (session_t * p_session)
{
    bool       b_valid       = true;
    uint16_t   username_size = 0u;
    uint16_t   password_size = 0u;
    uint8_t  * p_username    = NULL;
    uint8_t  * p_password    = NULL;

    if (
        (NULL == p_session) ||
        (NULL == p_session->p_username) ||
        (NULL == p_session->p_password)
    )
    {
        b_valid = false;
        goto cleanup;
    }

    username_size = p_session->username_size;
    password_size = p_session->password_size;
    p_username    = p_session->p_username;
    p_password    = p_session->p_password;

    if (!(
        (g_username_size_min <= username_size) &&
        (g_username_size_max >= username_size)
    ))
    {
        fprintf(stderr, "Username: 3-16 alphanumeric or underscore\n");
        b_valid = false;
        goto cleanup;
    }

    if (!(
        (g_password_size_min <= password_size) &&
        (g_password_size_max >= password_size)
    ))
    {
        fprintf(
            stderr,
            "Password: 8-128 printable characters excluding space\n"
        );

        b_valid = false;
        goto cleanup;
    }

    for (size_t idx = 0u; idx < username_size; idx++)
    {
        uint8_t chr = p_username[idx];
        if (!(isalnum(chr) || ('_' == chr)))
        {
            fprintf(stderr, "Username: 3-16 alphanumeric or underscore\n");
            b_valid = false;
            goto cleanup;
        }
    }

    for (size_t idx = 0u; idx < password_size; idx++)
    {
        uint8_t chr = p_password[idx];
        if (!(isprint(chr) && (' ' != chr)))
        {
            fprintf(
                stderr,
                "Password: 8-128 printable characters excluding space\n"
            );

            b_valid = false;
            goto cleanup;
        }
    }

cleanup:
    return b_valid;
}

/*** end of file ***/
