/**
 * @file test_copy_config.c
 * @author Michal Vasko <mvasko@cesnet.cz>
 * @brief test for sr_copy_config()
 *
 * @copyright
 * Copyright (c) 2019 CESNET, z.s.p.o.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#define _GNU_SOURCE

#include <sys/types.h>
#include <unistd.h>
#include <pthread.h>
#include <stdlib.h>
#include <setjmp.h>
#include <string.h>
#include <stdarg.h>

#include <cmocka.h>
#include <libyang/libyang.h>

#include "tests/config.h"
#include "sysrepo.h"

struct state {
    sr_conn_ctx_t *conn;
    volatile int cb_called;
    pthread_barrier_t barrier;
};

static int
setup(void **state)
{
    struct state *st;

    st = calloc(1, sizeof *st);
    *state = st;

    if (sr_connect("test1", 0, &(st->conn)) != SR_ERR_OK) {
        goto error;
    }

    if (sr_install_module(st->conn, TESTS_DIR "/files/test.yang", TESTS_DIR "/files", NULL, 0) != SR_ERR_OK) {
        goto error;
    }
    if (sr_install_module(st->conn, TESTS_DIR "/files/ietf-interfaces.yang", TESTS_DIR "/files", NULL, 0) != SR_ERR_OK) {
        goto error;
    }
    if (sr_install_module(st->conn, TESTS_DIR "/files/iana-if-type.yang", TESTS_DIR "/files", NULL, 0) != SR_ERR_OK) {
        goto error;
    }

    return 0;

error:
    sr_disconnect(st->conn);
    free(st);
    return 1;
}

static int
teardown(void **state)
{
    struct state *st = (struct state *)*state;

    sr_remove_module(st->conn, "ietf-interfaces");
    sr_remove_module(st->conn, "iana-if-type");
    sr_remove_module(st->conn, "test");

    sr_disconnect(st->conn);
    free(st);
    return 0;
}

static int
setup_f(void **state)
{
    struct state *st = (struct state *)*state;

    st->cb_called = 0;
    pthread_barrier_init(&st->barrier, NULL, 2);
    return 0;
}

static int
teardown_f(void **state)
{
    struct state *st = (struct state *)*state;
    sr_session_ctx_t *sess;

    sr_session_start(st->conn, SR_DS_RUNNING, 0, &sess);

    sr_delete_item(sess, "/ietf-interfaces:interfaces", 0);
    sr_delete_item(sess, "/test:l1[k='a']", 0);
    sr_delete_item(sess, "/test:l1[k='b']", 0);
    sr_delete_item(sess, "/test:ll1[.='1']", 0);
    sr_delete_item(sess, "/test:ll1[.='2']", 0);
    sr_delete_item(sess, "/test:cont", 0);
    sr_apply_changes(sess);

    sr_session_switch_ds(sess, SR_DS_STARTUP);

    sr_delete_item(sess, "/ietf-interfaces:interfaces", 0);
    sr_delete_item(sess, "/test:l1[k='a']", 0);
    sr_delete_item(sess, "/test:l1[k='b']", 0);
    sr_delete_item(sess, "/test:ll1[.='1']", 0);
    sr_delete_item(sess, "/test:ll1[.='2']", 0);
    sr_delete_item(sess, "/test:cont", 0);
    sr_apply_changes(sess);

    sr_session_stop(sess);

    pthread_barrier_destroy(&st->barrier);
    return 0;
}

/* TEST 1 */
static int
module_empty_cb(sr_session_ctx_t *session, const char *module_name, const char *xpath, sr_notif_event_t event,
        void *private_ctx)
{
    struct state *st = (struct state *)private_ctx;
    sr_change_oper_t op;
    sr_change_iter_t *iter;
    sr_val_t *old_val, *new_val;
    int ret;

    assert_string_equal(module_name, "ietf-interfaces");
    assert_null(xpath);

    switch (st->cb_called) {
    case 0:
    case 1:
        if (st->cb_called == 0) {
            assert_int_equal(event, SR_EV_CHANGE);
        } else {
            assert_int_equal(event, SR_EV_DONE);
        }

        /* get changes iter */
        ret = sr_get_changes_iter(session, "/ietf-interfaces:*//.", &iter);
        assert_int_equal(ret, SR_ERR_OK);

        /* 1st change */
        ret = sr_get_change_next(session, iter, &op, &old_val, &new_val);
        assert_int_equal(ret, SR_ERR_OK);

        assert_int_equal(op, SR_OP_CREATED);
        assert_null(old_val);
        assert_non_null(new_val);
        assert_string_equal(new_val->xpath, "/ietf-interfaces:interfaces/interface[name='eth1']");

        sr_free_val(new_val);

        /* 2nd change */
        ret = sr_get_change_next(session, iter, &op, &old_val, &new_val);
        assert_int_equal(ret, SR_ERR_OK);

        assert_int_equal(op, SR_OP_CREATED);
        assert_null(old_val);
        assert_non_null(new_val);
        assert_string_equal(new_val->xpath, "/ietf-interfaces:interfaces/interface[name='eth1']/name");

        sr_free_val(new_val);

        /* 3rd change */
        ret = sr_get_change_next(session, iter, &op, &old_val, &new_val);
        assert_int_equal(ret, SR_ERR_OK);

        assert_int_equal(op, SR_OP_CREATED);
        assert_null(old_val);
        assert_non_null(new_val);
        assert_string_equal(new_val->xpath, "/ietf-interfaces:interfaces/interface[name='eth1']/type");

        sr_free_val(new_val);

        /* 4th change */
        ret = sr_get_change_next(session, iter, &op, &old_val, &new_val);
        assert_int_equal(ret, SR_ERR_OK);

        assert_int_equal(op, SR_OP_CREATED);
        assert_null(old_val);
        assert_non_null(new_val);
        assert_string_equal(new_val->xpath, "/ietf-interfaces:interfaces/interface[name='eth1']/enabled");

        sr_free_val(new_val);

        /* 5th change */
        ret = sr_get_change_next(session, iter, &op, &old_val, &new_val);
        assert_int_equal(ret, SR_ERR_OK);

        assert_int_equal(op, SR_OP_CREATED);
        assert_null(old_val);
        assert_non_null(new_val);
        assert_string_equal(new_val->xpath, "/ietf-interfaces:interfaces/interface[name='eth2']");

        sr_free_val(new_val);

        /* 6th change */
        ret = sr_get_change_next(session, iter, &op, &old_val, &new_val);
        assert_int_equal(ret, SR_ERR_OK);

        assert_int_equal(op, SR_OP_CREATED);
        assert_null(old_val);
        assert_non_null(new_val);
        assert_string_equal(new_val->xpath, "/ietf-interfaces:interfaces/interface[name='eth2']/name");

        sr_free_val(new_val);

        /* 7th change */
        ret = sr_get_change_next(session, iter, &op, &old_val, &new_val);
        assert_int_equal(ret, SR_ERR_OK);

        assert_int_equal(op, SR_OP_CREATED);
        assert_null(old_val);
        assert_non_null(new_val);
        assert_string_equal(new_val->xpath, "/ietf-interfaces:interfaces/interface[name='eth2']/type");

        sr_free_val(new_val);

        /* 8th change */
        ret = sr_get_change_next(session, iter, &op, &old_val, &new_val);
        assert_int_equal(ret, SR_ERR_OK);

        assert_int_equal(op, SR_OP_CREATED);
        assert_null(old_val);
        assert_non_null(new_val);
        assert_string_equal(new_val->xpath, "/ietf-interfaces:interfaces/interface[name='eth2']/enabled");

        sr_free_val(new_val);

        /* no more changes */
        ret = sr_get_change_next(session, iter, &op, &old_val, &new_val);
        assert_int_equal(ret, SR_ERR_NOT_FOUND);

        sr_free_change_iter(iter);
        break;
    case 2:
    case 3:
        if (st->cb_called == 2) {
            assert_int_equal(event, SR_EV_CHANGE);
        } else {
            assert_int_equal(event, SR_EV_DONE);
        }

        /* get changes iter */
        ret = sr_get_changes_iter(session, "/ietf-interfaces:*//.", &iter);
        assert_int_equal(ret, SR_ERR_OK);

        /* 1st change */
        ret = sr_get_change_next(session, iter, &op, &old_val, &new_val);
        assert_int_equal(ret, SR_ERR_OK);

        assert_int_equal(op, SR_OP_DELETED);
        assert_non_null(old_val);
        assert_null(new_val);
        assert_string_equal(old_val->xpath, "/ietf-interfaces:interfaces/interface[name='eth1']");

        sr_free_val(old_val);

        /* 2nd change */
        ret = sr_get_change_next(session, iter, &op, &old_val, &new_val);
        assert_int_equal(ret, SR_ERR_OK);

        assert_int_equal(op, SR_OP_DELETED);
        assert_non_null(old_val);
        assert_null(new_val);
        assert_string_equal(old_val->xpath, "/ietf-interfaces:interfaces/interface[name='eth1']/name");

        sr_free_val(old_val);

        /* 3rd change */
        ret = sr_get_change_next(session, iter, &op, &old_val, &new_val);
        assert_int_equal(ret, SR_ERR_OK);

        assert_int_equal(op, SR_OP_DELETED);
        assert_non_null(old_val);
        assert_null(new_val);
        assert_string_equal(old_val->xpath, "/ietf-interfaces:interfaces/interface[name='eth1']/type");

        sr_free_val(old_val);

        /* 4th change */
        ret = sr_get_change_next(session, iter, &op, &old_val, &new_val);
        assert_int_equal(ret, SR_ERR_OK);

        assert_int_equal(op, SR_OP_DELETED);
        assert_non_null(old_val);
        assert_null(new_val);
        assert_string_equal(old_val->xpath, "/ietf-interfaces:interfaces/interface[name='eth1']/enabled");

        sr_free_val(old_val);

        /* 5th change */
        ret = sr_get_change_next(session, iter, &op, &old_val, &new_val);
        assert_int_equal(ret, SR_ERR_OK);

        assert_int_equal(op, SR_OP_DELETED);
        assert_non_null(old_val);
        assert_null(new_val);
        assert_string_equal(old_val->xpath, "/ietf-interfaces:interfaces/interface[name='eth2']");

        sr_free_val(old_val);

        /* 6th change */
        ret = sr_get_change_next(session, iter, &op, &old_val, &new_val);
        assert_int_equal(ret, SR_ERR_OK);

        assert_int_equal(op, SR_OP_DELETED);
        assert_non_null(old_val);
        assert_null(new_val);
        assert_string_equal(old_val->xpath, "/ietf-interfaces:interfaces/interface[name='eth2']/name");

        sr_free_val(old_val);

        /* 7th change */
        ret = sr_get_change_next(session, iter, &op, &old_val, &new_val);
        assert_int_equal(ret, SR_ERR_OK);

        assert_int_equal(op, SR_OP_DELETED);
        assert_non_null(old_val);
        assert_null(new_val);
        assert_string_equal(old_val->xpath, "/ietf-interfaces:interfaces/interface[name='eth2']/type");

        sr_free_val(old_val);

        /* 8th change */
        ret = sr_get_change_next(session, iter, &op, &old_val, &new_val);
        assert_int_equal(ret, SR_ERR_OK);

        assert_int_equal(op, SR_OP_DELETED);
        assert_non_null(old_val);
        assert_null(new_val);
        assert_string_equal(old_val->xpath, "/ietf-interfaces:interfaces/interface[name='eth2']/enabled");

        sr_free_val(old_val);

        /* no more changes */
        ret = sr_get_change_next(session, iter, &op, &old_val, &new_val);
        assert_int_equal(ret, SR_ERR_NOT_FOUND);

        sr_free_change_iter(iter);
        break;
    default:
        fail();
    }

    ++st->cb_called;
    return SR_ERR_OK;
}

static void *
copy_empty_thread(void *arg)
{
    struct state *st = (struct state *)arg;
    sr_session_ctx_t *sess;
    struct lyd_node *subtree;
    char *str1;
    const char *str2;
    int ret;

    ret = sr_session_start(st->conn, SR_DS_RUNNING, 0, &sess);
    assert_int_equal(ret, SR_ERR_OK);

    /* wait for subscription before copying */
    pthread_barrier_wait(&st->barrier);

    /* perform 1st copy-config */
    ret = sr_copy_config(sess, "ietf-interfaces", SR_DS_STARTUP, SR_DS_RUNNING);
    assert_int_equal(ret, SR_ERR_OK);

    /* check current data tree */
    ret = sr_get_subtree(sess, "/ietf-interfaces:interfaces", &subtree);
    assert_int_equal(ret, SR_ERR_OK);

    ret = lyd_print_mem(&str1, subtree, LYD_XML, LYP_WITHSIBLINGS);
    assert_int_equal(ret, 0);
    lyd_free(subtree);

    str2 =
    "<interfaces xmlns=\"urn:ietf:params:xml:ns:yang:ietf-interfaces\">"
        "<interface>"
            "<name>eth1</name>"
            "<type xmlns:ianaift=\"urn:ietf:params:xml:ns:yang:iana-if-type\">ianaift:ethernetCsmacd</type>"
        "</interface>"
        "<interface>"
            "<name>eth2</name>"
            "<type xmlns:ianaift=\"urn:ietf:params:xml:ns:yang:iana-if-type\">ianaift:ethernetCsmacd</type>"
        "</interface>"
    "</interfaces>";

    assert_string_equal(str1, str2);
    free(str1);

    /* clear startup data */
    ret = sr_session_switch_ds(sess, SR_DS_STARTUP);
    assert_int_equal(ret, SR_ERR_OK);

    ret = sr_delete_item(sess, "/ietf-interfaces:interfaces", 0);
    assert_int_equal(ret, SR_ERR_OK);
    ret = sr_apply_changes(sess);
    assert_int_equal(ret, SR_ERR_OK);

    ret = sr_session_switch_ds(sess, SR_DS_RUNNING);
    assert_int_equal(ret, SR_ERR_OK);

    /* perform 2nd copy-config */
    ret = sr_copy_config(sess, "ietf-interfaces", SR_DS_STARTUP, SR_DS_RUNNING);
    assert_int_equal(ret, SR_ERR_OK);

    /* check current data tree */
    ret = sr_get_subtree(sess, "/ietf-interfaces:interfaces", &subtree);
    assert_int_equal(ret, SR_ERR_OK);

    ret = lyd_print_mem(&str1, subtree, LYD_XML, LYP_WITHSIBLINGS);
    assert_int_equal(ret, 0);

    assert_null(str1);
    lyd_free(subtree);

    /* signal that we have finished copying */
    pthread_barrier_wait(&st->barrier);

    sr_session_stop(sess);
    return NULL;
}

static void *
subscribe_empty_thread(void *arg)
{
    struct state *st = (struct state *)arg;
    sr_session_ctx_t *sess;
    sr_subscription_ctx_t *subscr;
    int count, ret;

    ret = sr_session_start(st->conn, SR_DS_RUNNING, 0, &sess);
    assert_int_equal(ret, SR_ERR_OK);

    ret = sr_module_change_subscribe(sess, "ietf-interfaces", NULL, module_empty_cb, st, 0, 0, &subscr);
    assert_int_equal(ret, SR_ERR_OK);

    /* set some startup data */
    ret = sr_session_switch_ds(sess, SR_DS_STARTUP);
    assert_int_equal(ret, SR_ERR_OK);

    ret = sr_set_item_str(sess, "/ietf-interfaces:interfaces/interface[name='eth1']/type", "iana-if-type:ethernetCsmacd", 0);
    assert_int_equal(ret, SR_ERR_OK);
    ret = sr_set_item_str(sess, "/ietf-interfaces:interfaces/interface[name='eth2']/type", "iana-if-type:ethernetCsmacd", 0);
    assert_int_equal(ret, SR_ERR_OK);
    ret = sr_apply_changes(sess);
    assert_int_equal(ret, SR_ERR_OK);

    ret = sr_session_switch_ds(sess, SR_DS_RUNNING);
    assert_int_equal(ret, SR_ERR_OK);

    /* signal that subscription was created */
    pthread_barrier_wait(&st->barrier);

    count = 0;
    while ((st->cb_called < 4) && (count < 1500)) {
        usleep(10000);
        ++count;
    }
    assert_int_equal(st->cb_called, 4);

    /* wait for the other thread to finish */
    pthread_barrier_wait(&st->barrier);

    sr_unsubscribe(subscr);
    sr_session_stop(sess);
    return NULL;
}

static void
test_empty(void **state)
{
    pthread_t tid[2];

    pthread_create(&tid[0], NULL, copy_empty_thread, *state);
    pthread_create(&tid[1], NULL, subscribe_empty_thread, *state);

    pthread_join(tid[0], NULL);
    pthread_join(tid[1], NULL);
}

/* TEST 2 */
static int
module_simple_cb(sr_session_ctx_t *session, const char *module_name, const char *xpath, sr_notif_event_t event,
        void *private_ctx)
{
    struct state *st = (struct state *)private_ctx;
    sr_change_oper_t op;
    sr_change_iter_t *iter;
    sr_val_t *old_val, *new_val;
    int ret;

    assert_string_equal(module_name, "ietf-interfaces");
    assert_null(xpath);

    switch (st->cb_called) {
    case 0:
    case 1:
        if (st->cb_called == 0) {
            assert_int_equal(event, SR_EV_CHANGE);
        } else {
            assert_int_equal(event, SR_EV_DONE);
        }

        /* get changes iter */
        ret = sr_get_changes_iter(session, "/ietf-interfaces:*//.", &iter);
        assert_int_equal(ret, SR_ERR_OK);

        /* 1st change */
        ret = sr_get_change_next(session, iter, &op, &old_val, &new_val);
        assert_int_equal(ret, SR_ERR_OK);

        assert_int_equal(op, SR_OP_MODIFIED);
        assert_non_null(old_val);
        assert_string_equal(old_val->xpath, "/ietf-interfaces:interfaces/interface[name='eth1']/type");
        assert_string_equal(old_val->data.string_val, "iana-if-type:ethernetCsmacd");
        assert_non_null(new_val);
        assert_string_equal(new_val->xpath, "/ietf-interfaces:interfaces/interface[name='eth1']/type");
        assert_string_equal(new_val->data.string_val, "iana-if-type:sonet");

        sr_free_val(old_val);
        sr_free_val(new_val);

        /* 2nd change */
        ret = sr_get_change_next(session, iter, &op, &old_val, &new_val);
        assert_int_equal(ret, SR_ERR_OK);

        assert_int_equal(op, SR_OP_CREATED);
        assert_null(old_val);
        assert_non_null(new_val);
        assert_string_equal(new_val->xpath, "/ietf-interfaces:interfaces/interface[name='eth1']/description");

        sr_free_val(new_val);

        /* 3rd change */
        ret = sr_get_change_next(session, iter, &op, &old_val, &new_val);
        assert_int_equal(ret, SR_ERR_OK);

        assert_int_equal(op, SR_OP_MODIFIED);
        assert_non_null(old_val);
        assert_string_equal(old_val->xpath, "/ietf-interfaces:interfaces/interface[name='eth2']/enabled");
        assert_int_equal(old_val->data.bool_val, 1);
        assert_non_null(new_val);
        assert_string_equal(new_val->xpath, "/ietf-interfaces:interfaces/interface[name='eth2']/enabled");
        assert_int_equal(new_val->data.bool_val, 0);

        sr_free_val(old_val);
        sr_free_val(new_val);

        /* no more changes */
        ret = sr_get_change_next(session, iter, &op, &old_val, &new_val);
        assert_int_equal(ret, SR_ERR_NOT_FOUND);

        sr_free_change_iter(iter);
        break;
    case 2:
    case 3:
        if (st->cb_called == 2) {
            assert_int_equal(event, SR_EV_CHANGE);
        } else {
            assert_int_equal(event, SR_EV_DONE);
        }

        /* get changes iter */
        ret = sr_get_changes_iter(session, "/ietf-interfaces:*//.", &iter);
        assert_int_equal(ret, SR_ERR_OK);

        /* 1st change */
        ret = sr_get_change_next(session, iter, &op, &old_val, &new_val);
        assert_int_equal(ret, SR_ERR_OK);

        assert_int_equal(op, SR_OP_MODIFIED);
        assert_non_null(old_val);
        assert_string_equal(old_val->xpath, "/ietf-interfaces:interfaces/interface[name='eth2']/enabled");
        assert_int_equal(old_val->data.bool_val, 0);
        assert_int_equal(old_val->dflt, 0);
        assert_non_null(new_val);
        assert_string_equal(new_val->xpath, "/ietf-interfaces:interfaces/interface[name='eth2']/enabled");
        assert_int_equal(new_val->data.bool_val, 1);
        assert_int_equal(new_val->dflt, 1);

        sr_free_val(old_val);
        sr_free_val(new_val);

        /* 2nd change */
        ret = sr_get_change_next(session, iter, &op, &old_val, &new_val);
        assert_int_equal(ret, SR_ERR_OK);

        assert_int_equal(op, SR_OP_DELETED);
        assert_non_null(old_val);
        assert_null(new_val);
        assert_string_equal(old_val->xpath, "/ietf-interfaces:interfaces/interface[name='eth1']");

        sr_free_val(old_val);

        /* 3rd change */
        ret = sr_get_change_next(session, iter, &op, &old_val, &new_val);
        assert_int_equal(ret, SR_ERR_OK);

        assert_int_equal(op, SR_OP_DELETED);
        assert_non_null(old_val);
        assert_null(new_val);
        assert_string_equal(old_val->xpath, "/ietf-interfaces:interfaces/interface[name='eth1']/name");

        sr_free_val(old_val);

        /* 4th change */
        ret = sr_get_change_next(session, iter, &op, &old_val, &new_val);
        assert_int_equal(ret, SR_ERR_OK);

        assert_int_equal(op, SR_OP_DELETED);
        assert_non_null(old_val);
        assert_null(new_val);
        assert_string_equal(old_val->xpath, "/ietf-interfaces:interfaces/interface[name='eth1']/type");

        sr_free_val(old_val);

        /* 5th change */
        ret = sr_get_change_next(session, iter, &op, &old_val, &new_val);
        assert_int_equal(ret, SR_ERR_OK);

        assert_int_equal(op, SR_OP_DELETED);
        assert_non_null(old_val);
        assert_null(new_val);
        assert_string_equal(old_val->xpath, "/ietf-interfaces:interfaces/interface[name='eth1']/enabled");

        sr_free_val(old_val);

        /* 6th change */
        ret = sr_get_change_next(session, iter, &op, &old_val, &new_val);
        assert_int_equal(ret, SR_ERR_OK);

        assert_int_equal(op, SR_OP_DELETED);
        assert_non_null(old_val);
        assert_null(new_val);
        assert_string_equal(old_val->xpath, "/ietf-interfaces:interfaces/interface[name='eth1']/description");

        sr_free_val(old_val);

        /* no more changes */
        ret = sr_get_change_next(session, iter, &op, &old_val, &new_val);
        assert_int_equal(ret, SR_ERR_NOT_FOUND);

        sr_free_change_iter(iter);
        break;
    default:
        fail();
    }

    ++st->cb_called;
    return SR_ERR_OK;
}

static void *
copy_simple_thread(void *arg)
{
    struct state *st = (struct state *)arg;
    sr_session_ctx_t *sess;
    struct lyd_node *subtree;
    char *str1;
    const char *str2;
    int ret;

    ret = sr_session_start(st->conn, SR_DS_STARTUP, 0, &sess);
    assert_int_equal(ret, SR_ERR_OK);

    /* wait for subscription before copying */
    pthread_barrier_wait(&st->barrier);

    /* perform some startup changes */
    ret = sr_set_item_str(sess, "/ietf-interfaces:interfaces/interface[name='eth1']/description", "some-eth1-desc", 0);
    assert_int_equal(ret, SR_ERR_OK);
    ret = sr_set_item_str(sess, "/ietf-interfaces:interfaces/interface[name='eth1']/type", "iana-if-type:sonet", 0);
    assert_int_equal(ret, SR_ERR_OK);
    ret = sr_set_item_str(sess, "/ietf-interfaces:interfaces/interface[name='eth2']/enabled", "false", 0);
    assert_int_equal(ret, SR_ERR_OK);
    ret = sr_apply_changes(sess);
    assert_int_equal(ret, SR_ERR_OK);

    /* perform 1st copy-config */
    ret = sr_copy_config(sess, "ietf-interfaces", SR_DS_STARTUP, SR_DS_RUNNING);
    assert_int_equal(ret, SR_ERR_OK);

    /* check current data tree */
    ret = sr_get_subtree(sess, "/ietf-interfaces:interfaces", &subtree);
    assert_int_equal(ret, SR_ERR_OK);

    ret = lyd_print_mem(&str1, subtree, LYD_XML, LYP_WITHSIBLINGS);
    assert_int_equal(ret, 0);
    lyd_free(subtree);

    str2 =
    "<interfaces xmlns=\"urn:ietf:params:xml:ns:yang:ietf-interfaces\">"
        "<interface>"
            "<name>eth1</name>"
            "<type xmlns:ianaift=\"urn:ietf:params:xml:ns:yang:iana-if-type\">ianaift:sonet</type>"
            "<description>some-eth1-desc</description>"
        "</interface>"
        "<interface>"
            "<name>eth2</name>"
            "<type xmlns:ianaift=\"urn:ietf:params:xml:ns:yang:iana-if-type\">ianaift:ethernetCsmacd</type>"
            "<enabled>false</enabled>"
        "</interface>"
    "</interfaces>";

    assert_string_equal(str1, str2);
    free(str1);

    /* perform some startup changes */
    ret = sr_delete_item(sess, "/ietf-interfaces:interfaces/interface[name='eth1']", 0);
    assert_int_equal(ret, SR_ERR_OK);
    ret = sr_delete_item(sess, "/ietf-interfaces:interfaces/interface[name='eth2']/enabled", 0);
    assert_int_equal(ret, SR_ERR_OK);
    ret = sr_apply_changes(sess);
    assert_int_equal(ret, SR_ERR_OK);

    /* perform 2nd copy-config */
    ret = sr_copy_config(sess, "ietf-interfaces", SR_DS_STARTUP, SR_DS_RUNNING);
    assert_int_equal(ret, SR_ERR_OK);

    /* check current data tree */
    ret = sr_get_subtree(sess, "/ietf-interfaces:interfaces", &subtree);
    assert_int_equal(ret, SR_ERR_OK);

    ret = lyd_print_mem(&str1, subtree, LYD_XML, LYP_WITHSIBLINGS);
    assert_int_equal(ret, 0);
    lyd_free(subtree);

    str2 =
    "<interfaces xmlns=\"urn:ietf:params:xml:ns:yang:ietf-interfaces\">"
        "<interface>"
            "<name>eth2</name>"
            "<type xmlns:ianaift=\"urn:ietf:params:xml:ns:yang:iana-if-type\">ianaift:ethernetCsmacd</type>"
        "</interface>"
    "</interfaces>";

    assert_string_equal(str1, str2);
    free(str1);

    /* signal that we have finished copying */
    pthread_barrier_wait(&st->barrier);

    sr_session_stop(sess);
    return NULL;
}

static void *
subscribe_simple_thread(void *arg)
{
    struct state *st = (struct state *)arg;
    sr_session_ctx_t *sess;
    sr_subscription_ctx_t *subscr;
    int count, ret;

    ret = sr_session_start(st->conn, SR_DS_RUNNING, 0, &sess);
    assert_int_equal(ret, SR_ERR_OK);

    /* set the same running and startup data */
    ret = sr_set_item_str(sess, "/ietf-interfaces:interfaces/interface[name='eth1']/type", "iana-if-type:ethernetCsmacd", 0);
    assert_int_equal(ret, SR_ERR_OK);
    ret = sr_set_item_str(sess, "/ietf-interfaces:interfaces/interface[name='eth2']/type", "iana-if-type:ethernetCsmacd", 0);
    assert_int_equal(ret, SR_ERR_OK);
    ret = sr_apply_changes(sess);
    assert_int_equal(ret, SR_ERR_OK);

    ret = sr_session_switch_ds(sess, SR_DS_STARTUP);
    assert_int_equal(ret, SR_ERR_OK);

    ret = sr_set_item_str(sess, "/ietf-interfaces:interfaces/interface[name='eth1']/type", "iana-if-type:ethernetCsmacd", 0);
    assert_int_equal(ret, SR_ERR_OK);
    ret = sr_set_item_str(sess, "/ietf-interfaces:interfaces/interface[name='eth2']/type", "iana-if-type:ethernetCsmacd", 0);
    assert_int_equal(ret, SR_ERR_OK);
    ret = sr_apply_changes(sess);
    assert_int_equal(ret, SR_ERR_OK);

    ret = sr_session_switch_ds(sess, SR_DS_RUNNING);
    assert_int_equal(ret, SR_ERR_OK);

    /* subscribe */
    ret = sr_module_change_subscribe(sess, "ietf-interfaces", NULL, module_simple_cb, st, 0, 0, &subscr);
    assert_int_equal(ret, SR_ERR_OK);

    /* signal that subscription was created */
    pthread_barrier_wait(&st->barrier);

    count = 0;
    while ((st->cb_called < 4) && (count < 1500)) {
        usleep(10000);
        ++count;
    }
    assert_int_equal(st->cb_called, 4);

    /* wait for the other thread to finish */
    pthread_barrier_wait(&st->barrier);

    sr_unsubscribe(subscr);
    sr_session_stop(sess);
    return NULL;
}

static void
test_simple(void **state)
{
    pthread_t tid[2];

    pthread_create(&tid[0], NULL, copy_simple_thread, *state);
    pthread_create(&tid[1], NULL, subscribe_simple_thread, *state);

    pthread_join(tid[0], NULL);
    pthread_join(tid[1], NULL);
}

/* TEST 3 */
static int
module_userord_cb(sr_session_ctx_t *session, const char *module_name, const char *xpath, sr_notif_event_t event,
        void *private_ctx)
{
    struct state *st = (struct state *)private_ctx;
    sr_change_oper_t op;
    sr_change_iter_t *iter;
    sr_val_t *old_val, *new_val;
    int ret;

    assert_string_equal(module_name, "test");
    assert_null(xpath);

    switch (st->cb_called) {
    case 0:
    case 1:
        if (st->cb_called == 0) {
            assert_int_equal(event, SR_EV_CHANGE);
        } else {
            assert_int_equal(event, SR_EV_DONE);
        }

        /* get changes iter */
        ret = sr_get_changes_iter(session, "/test:*//.", &iter);
        assert_int_equal(ret, SR_ERR_OK);

        /* 1st change */
        ret = sr_get_change_next(session, iter, &op, &old_val, &new_val);
        assert_int_equal(ret, SR_ERR_OK);

        assert_int_equal(op, SR_OP_MOVED);
        assert_non_null(old_val);
        assert_string_equal(old_val->xpath, "/test:l1[k='b']");
        assert_non_null(new_val);
        assert_string_equal(new_val->xpath, "/test:l1[k='a']");

        sr_free_val(old_val);
        sr_free_val(new_val);

        /* 2nd change */
        ret = sr_get_change_next(session, iter, &op, &old_val, &new_val);
        assert_int_equal(ret, SR_ERR_OK);

        assert_int_equal(op, SR_OP_MOVED);
        assert_non_null(old_val);
        assert_string_equal(old_val->xpath, "/test:cont/ll2[.='2']");
        assert_non_null(new_val);
        assert_string_equal(new_val->xpath, "/test:cont/ll2[.='1']");

        sr_free_val(old_val);
        sr_free_val(new_val);

        /* no more changes */
        ret = sr_get_change_next(session, iter, &op, &old_val, &new_val);
        assert_int_equal(ret, SR_ERR_NOT_FOUND);

        sr_free_change_iter(iter);
        break;
    default:
        fail();
    }

    ++st->cb_called;
    return SR_ERR_OK;
}

static void *
copy_userord_thread(void *arg)
{
    struct state *st = (struct state *)arg;
    sr_session_ctx_t *sess;
    struct ly_set *subtrees;
    struct lyd_node *node;
    int ret;

    ret = sr_session_start(st->conn, SR_DS_STARTUP, 0, &sess);
    assert_int_equal(ret, SR_ERR_OK);

    /* wait for subscription before copying */
    pthread_barrier_wait(&st->barrier);

    /* perform some startup changes */
    ret = sr_move_item(sess, "/test:l1[k='a']", SR_MOVE_AFTER, "[k='b']", NULL);
    assert_int_equal(ret, SR_ERR_OK);
    ret = sr_move_item(sess, "/test:cont/ll2[.='1']", SR_MOVE_AFTER, NULL, "2");
    assert_int_equal(ret, SR_ERR_OK);
    ret = sr_apply_changes(sess);
    assert_int_equal(ret, SR_ERR_OK);

    /* perform 1st copy-config */
    ret = sr_copy_config(sess, "test", SR_DS_STARTUP, SR_DS_RUNNING);
    assert_int_equal(ret, SR_ERR_OK);

    /* check current data tree */
    ret = sr_get_subtrees(sess, "/test:*", &subtrees);
    assert_int_equal(ret, SR_ERR_OK);
    assert_int_equal(subtrees->number, 5);

    node = subtrees->set.d[0];
    assert_string_equal(node->schema->name, "cont");
    node = node->child;
    assert_string_equal(node->schema->name, "l2");
    assert_string_equal(((struct lyd_node_leaf_list *)node->child)->value_str, "a");
    node = node->next;
    assert_string_equal(node->schema->name, "l2");
    assert_string_equal(((struct lyd_node_leaf_list *)node->child)->value_str, "b");
    node = node->next;
    assert_string_equal(node->schema->name, "ll2");
    assert_string_equal(((struct lyd_node_leaf_list *)node)->value_str, "2");
    node = node->next;
    assert_string_equal(node->schema->name, "ll2");
    assert_string_equal(((struct lyd_node_leaf_list *)node)->value_str, "1");
    assert_null(node->next);
    lyd_free_withsiblings(node->parent);

    node = subtrees->set.d[1];
    assert_string_equal(node->schema->name, "ll1");
    assert_string_equal(((struct lyd_node_leaf_list *)node)->value_str, "1");
    lyd_free_withsiblings(node);

    node = subtrees->set.d[2];
    assert_string_equal(node->schema->name, "l1");
    assert_string_equal(((struct lyd_node_leaf_list *)node->child)->value_str, "b");
    lyd_free_withsiblings(node);

    node = subtrees->set.d[3];
    assert_string_equal(node->schema->name, "l1");
    assert_string_equal(((struct lyd_node_leaf_list *)node->child)->value_str, "a");
    lyd_free_withsiblings(node);

    node = subtrees->set.d[4];
    assert_string_equal(node->schema->name, "ll1");
    assert_string_equal(((struct lyd_node_leaf_list *)node)->value_str, "2");
    lyd_free_withsiblings(node);

    ly_set_free(subtrees);

    /* perform some startup changes (no actual changes) */
    ret = sr_move_item(sess, "/test:ll1[.='1']", SR_MOVE_BEFORE, NULL, "2");
    assert_int_equal(ret, SR_ERR_OK);
    ret = sr_move_item(sess, "/test:cont/l2[k='a']", SR_MOVE_BEFORE, "[k='b']", NULL);
    assert_int_equal(ret, SR_ERR_OK);
    ret = sr_apply_changes(sess);
    assert_int_equal(ret, SR_ERR_OK);

    /* perform 2nd copy-config (no changes) */
    ret = sr_copy_config(sess, "test", SR_DS_STARTUP, SR_DS_RUNNING);
    assert_int_equal(ret, SR_ERR_OK);

    /* check current data tree (should be the same) */
    ret = sr_get_subtrees(sess, "/test:*", &subtrees);
    assert_int_equal(ret, SR_ERR_OK);
    assert_int_equal(subtrees->number, 5);

    node = subtrees->set.d[0];
    assert_string_equal(node->schema->name, "cont");
    node = node->child;
    assert_string_equal(node->schema->name, "l2");
    assert_string_equal(((struct lyd_node_leaf_list *)node->child)->value_str, "a");
    node = node->next;
    assert_string_equal(node->schema->name, "l2");
    assert_string_equal(((struct lyd_node_leaf_list *)node->child)->value_str, "b");
    node = node->next;
    assert_string_equal(node->schema->name, "ll2");
    assert_string_equal(((struct lyd_node_leaf_list *)node)->value_str, "2");
    node = node->next;
    assert_string_equal(node->schema->name, "ll2");
    assert_string_equal(((struct lyd_node_leaf_list *)node)->value_str, "1");
    assert_null(node->next);
    lyd_free_withsiblings(node->parent);

    node = subtrees->set.d[1];
    assert_string_equal(node->schema->name, "ll1");
    assert_string_equal(((struct lyd_node_leaf_list *)node)->value_str, "1");
    lyd_free_withsiblings(node);

    node = subtrees->set.d[2];
    assert_string_equal(node->schema->name, "l1");
    assert_string_equal(((struct lyd_node_leaf_list *)node->child)->value_str, "b");
    lyd_free_withsiblings(node);

    node = subtrees->set.d[3];
    assert_string_equal(node->schema->name, "l1");
    assert_string_equal(((struct lyd_node_leaf_list *)node->child)->value_str, "a");
    lyd_free_withsiblings(node);

    node = subtrees->set.d[4];
    assert_string_equal(node->schema->name, "ll1");
    assert_string_equal(((struct lyd_node_leaf_list *)node)->value_str, "2");
    lyd_free_withsiblings(node);

    ly_set_free(subtrees);

    /* signal that we have finished copying */
    pthread_barrier_wait(&st->barrier);

    sr_session_stop(sess);
    return NULL;
}

static void *
subscribe_userord_thread(void *arg)
{
    struct state *st = (struct state *)arg;
    sr_session_ctx_t *sess;
    sr_subscription_ctx_t *subscr;
    int count, ret;

    ret = sr_session_start(st->conn, SR_DS_RUNNING, 0, &sess);
    assert_int_equal(ret, SR_ERR_OK);

    /* set the same running and startup data */
    ret = sr_set_item_str(sess, "/test:l1[k='a']/v", "1", 0);
    assert_int_equal(ret, SR_ERR_OK);
    ret = sr_set_item_str(sess, "/test:ll1[.='1']", NULL, 0);
    assert_int_equal(ret, SR_ERR_OK);
    ret = sr_set_item_str(sess, "/test:l1[k='b']/v", "2", 0);
    assert_int_equal(ret, SR_ERR_OK);
    ret = sr_set_item_str(sess, "/test:ll1[.='2']", NULL, 0);
    assert_int_equal(ret, SR_ERR_OK);

    ret = sr_set_item_str(sess, "/test:cont/l2[k='a']/v", "1", 0);
    assert_int_equal(ret, SR_ERR_OK);
    ret = sr_set_item_str(sess, "/test:cont/ll2[.='1']", NULL, 0);
    assert_int_equal(ret, SR_ERR_OK);
    ret = sr_set_item_str(sess, "/test:cont/l2[k='b']/v", "2", 0);
    assert_int_equal(ret, SR_ERR_OK);
    ret = sr_set_item_str(sess, "/test:cont/ll2[.='2']", NULL, 0);
    assert_int_equal(ret, SR_ERR_OK);

    ret = sr_apply_changes(sess);
    assert_int_equal(ret, SR_ERR_OK);

    ret = sr_copy_config(sess, "test", SR_DS_RUNNING, SR_DS_STARTUP);
    assert_int_equal(ret, SR_ERR_OK);

    /* subscribe */
    ret = sr_module_change_subscribe(sess, "test", NULL, module_userord_cb, st, 0, 0, &subscr);
    assert_int_equal(ret, SR_ERR_OK);

    /* signal that subscription was created */
    pthread_barrier_wait(&st->barrier);

    count = 0;
    while ((st->cb_called < 2) && (count < 1500)) {
        usleep(10000);
        ++count;
    }
    assert_int_equal(st->cb_called, 2);

    /* wait for the other thread to finish */
    pthread_barrier_wait(&st->barrier);

    sr_unsubscribe(subscr);
    sr_session_stop(sess);
    return NULL;
}

static void
test_userord(void **state)
{
    pthread_t tid[2];

    pthread_create(&tid[0], NULL, copy_userord_thread, *state);
    pthread_create(&tid[1], NULL, subscribe_userord_thread, *state);

    pthread_join(tid[0], NULL);
    pthread_join(tid[1], NULL);
}

/* MAIN */
int
main(void)
{
    const struct CMUnitTest tests[] = {
        cmocka_unit_test_setup_teardown(test_empty, setup_f, teardown_f),
        cmocka_unit_test_setup_teardown(test_simple, setup_f, teardown_f),
        cmocka_unit_test_setup_teardown(test_userord, setup_f, teardown_f),
    };

    sr_log_stderr(SR_LL_INF);
    return cmocka_run_group_tests(tests, setup, teardown);
}