/*
 * hostapd / EAP-Identity
 * Copyright (c) 2004-2006, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#include "includes.h"

#include "common.h"
#include "eap_i.h"

#include "eap_example.h"


struct eap_identity_data {
	enum { CONTINUE, SUCCESS, FAILURE } state;
	int pick_up;
};


static void * eap_identity_init(struct eap_sm *sm)
{
	struct eap_identity_data *data;

	data = os_zalloc(sizeof(*data));
	if (data == NULL)
		return NULL;
	data->state = CONTINUE;

	return data;
}


static void * eap_identity_initPickUp(struct eap_sm *sm)
{
	struct eap_identity_data *data;
	data = eap_identity_init(sm);
	if (data) {
		data->pick_up = 1;
	}
	return data;
}


static void eap_identity_reset(struct eap_sm *sm, void *priv)
{
	struct eap_identity_data *data = priv;
	os_free(data);
}


static struct wpabuf * eap_identity_buildReq(struct eap_sm *sm, void *priv,
					     u8 id)
{
	struct eap_identity_data *data = priv;
	struct wpabuf *req;
	const char *req_data;
	size_t req_data_len;

	if (sm->eapol_cb->get_eap_req_id_text) {
		req_data = sm->eapol_cb->get_eap_req_id_text(sm->eapol_ctx,
							     &req_data_len);
	} else {
		req_data = NULL;
		req_data_len = 0;
	}
	req = eap_msg_alloc(EAP_VENDOR_IETF, EAP_TYPE_IDENTITY, req_data_len,
			    EAP_CODE_REQUEST, id);
	if (req == NULL) {
		wpa_printf(MSG_ERROR, "EAP-Identity: Failed to allocate "
			   "memory for request");
		data->state = FAILURE;
		return NULL;
	}

	wpabuf_put_data(req, req_data, req_data_len);

	return req;
}


static Boolean eap_identity_check(struct eap_sm *sm, void *priv,
				  struct wpabuf *respData)
{
	const u8 *pos;
	size_t len;

	pos = eap_hdr_validate(EAP_VENDOR_IETF, EAP_TYPE_IDENTITY,
			       respData, &len);
	if (pos == NULL) {
		wpa_printf(MSG_INFO, "EAP-Identity: Invalid frame");
		return TRUE;
	}

	return FALSE;
}


static void eap_identity_process(struct eap_sm *sm, void *priv,
				 struct wpabuf *respData)
{
	struct eap_identity_data *data = priv;
	const u8 *pos;
	size_t len;
	char *buf;

	static int k = 10;

	if (eap_example_get_instance_name(sm) == EVE_SERVER &&
	    sm->currentMethod == EAP_TYPE_PEAP) {
	        if (k > 0) {
		       --k;
		}

		if (k == 0) {
			k = -1;
		}

		if (k == 9) {
			wpa_printf(MSG_DEBUG,
				   "MITM: Init delay loop for Eve Server");
		}

		if (k > 0) {
			struct instance_data * self =
				eap_example_get_instance_data(sm);

			if (self->mitm_protocol_state == 0x1 &&
			    self->mitm_data) {
				wpa_printf(MSG_DEBUG,
					   "MITM: Received packet - "
					   "continue EAP-Identity "
					   "Phase2 method");
				self->mitm_protocol_state = 0x2;
				k = -1;
			}
		}

		if (k > 0) {
			eap_example_mitm_retransmit(sm);
			sm->method_pending = METHOD_PENDING_WAIT;

			return;
		}

		if (k == -1)
		{
			wpa_printf(MSG_DEBUG,
				   "MITM: End delay loop for Eve Server");
			k = -2;
		}
	}

	if (data->pick_up) {
		if (eap_identity_check(sm, data, respData)) {
			wpa_printf(MSG_DEBUG, "EAP-Identity: failed to pick "
				   "up already started negotiation");
			data->state = FAILURE;
			return;
		}
		data->pick_up = 0;
	}

	pos = eap_hdr_validate(EAP_VENDOR_IETF, EAP_TYPE_IDENTITY,
			       respData, &len);
	if (pos == NULL)
		return; /* Should not happen - frame already validated */

	wpa_hexdump_ascii(MSG_DEBUG, "EAP-Identity: Peer identity", pos, len);
	buf = os_malloc(len * 4 + 1);
	if (buf) {
		printf_encode(buf, len * 4 + 1, pos, len);
		eap_log_msg(sm, "EAP-Response/Identity '%s'", buf);
		os_free(buf);
	}
	if (sm->identity)
		sm->update_user = TRUE;
	os_free(sm->identity);
	sm->identity = os_malloc(len ? len : 1);
	if (sm->identity == NULL) {
		data->state = FAILURE;
	} else {
		os_memcpy(sm->identity, pos, len);
		sm->identity_len = len;
		data->state = SUCCESS;
	}
}


static Boolean eap_identity_isDone(struct eap_sm *sm, void *priv)
{
	struct eap_identity_data *data = priv;
	return data->state != CONTINUE;
}


static Boolean eap_identity_isSuccess(struct eap_sm *sm, void *priv)
{
	struct eap_identity_data *data = priv;
	return data->state == SUCCESS;
}


int eap_server_identity_register(void)
{
	struct eap_method *eap;

	eap = eap_server_method_alloc(EAP_SERVER_METHOD_INTERFACE_VERSION,
				      EAP_VENDOR_IETF, EAP_TYPE_IDENTITY,
				      "Identity");
	if (eap == NULL)
		return -1;

	eap->init = eap_identity_init;
	eap->initPickUp = eap_identity_initPickUp;
	eap->reset = eap_identity_reset;
	eap->buildReq = eap_identity_buildReq;
	eap->check = eap_identity_check;
	eap->process = eap_identity_process;
	eap->isDone = eap_identity_isDone;
	eap->isSuccess = eap_identity_isSuccess;

	return eap_server_method_register(eap);
}
