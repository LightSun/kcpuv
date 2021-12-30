#include "header.h"
#include "conn_client.h"
#include "rand.h"
#include "tm.h"

ConnClient::ConnClient(Network* network) 
	: Conn(network) {
	_next_ping_tick = 0;
	_req_conn_times = 0;
	_next_req_conn_tick = 0;
}

ConnClient::~ConnClient() {

}

int ConnClient::run(uint64_t tick) {
	return Conn::run(tick);
}

int ConnClient::prepare_req_conn(const sockaddr* addr, uv_udp_t* handle) {
	_addr = *addr;
	_udp = handle;

	_status = CONV_REQ_CONN;
	_next_req_conn_tick = 0;

	_req_conn_times = 10;
	_req_conn.conv = CONV_REQ_CONN;
	_req_conn.n = rand_uint32();

	return 0;
}

void ConnClient::req_conn_run() {
	uint64_t now = get_tick_ms();
	if (_status == CONV_REQ_CONN) {
		if (_req_conn_times > 0) {
			if (now > _next_req_conn_tick) {
				send_udp((const char*)&_req_conn, sizeof(_req_conn));
				_req_conn_times--;
				_next_req_conn_tick = now + 1000;
			}
		}
	}
}

void ConnClient::on_recv_udp(const char* buf, ssize_t size, const struct sockaddr* addr) {
	if (_status == CONV_REQ_CONN) {
		on_recv_udp_snd_conv(buf, size);
	} else {
		Conn::on_recv_udp(buf, size, addr);
	}
}

void ConnClient::on_recv_udp_snd_conv(const char* buf, ssize_t size) {
	int r = -1;
	kcpuv_conv_t conv;
	hs_snd_conv_s* hs = NULL;

    conv = (kcpuv_conv_t)ikcp_getconv(buf);
    PROC_ERR_NOLOG(conv);

	CHK_COND_NOLOG(conv == CONV_SND_CONV);
	CHK_COND_NOLOG(sizeof(hs_snd_conv_s) == size);

	hs = (hs_snd_conv_s*)buf;
	CHK_COND_NOLOG(hs->n == _req_conn.n);

	r = init_kcp(hs->new_conv);
	PROC_ERR_NOLOG(r);

	_conv = hs->new_conv;
	_key = hs->key;

	hs_ack_conv_s ack;
	ack.header.key = _key;
	r = send_kcp_raw((const char*)&ack, sizeof(ack));
	PROC_ERR(r);

	_status = CONV_ESTABLISHED;
	alive();
Exit0:
	return;
}

int ConnClient::recv_kcp(char*& buf, uint32_t& size) {
	int r = Conn::recv_kcp(buf, size);
	if (r == 0 && size == 0 && buf == NULL) {
		return 1;
	}
	return r;
}
