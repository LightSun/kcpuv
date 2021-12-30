#include "test_common.cpp"
#include <map>

static const int MAX_Test_Msg_SIZE = 1024 * 64;

typedef struct Test_Msg {
	uint64_t tick;
    char data[MAX_Test_Msg_SIZE];
} Test_Msg;

static std::map<uint64_t, Test_Msg> snd_Test_Msg_map;

static int make_Test_Msg(uint64_t tick, Test_Msg& Test_Msg, int size_min, int size_max) {
    Test_Msg.tick = tick;
	uint32_t size = (rand_u32() % (size_max - size_min + 1)) + size_min;
	for (uint32_t i = 0; i < size; ++i) {
        Test_Msg.data[i] = 'a' + (rand_u32() % 26);
	}
	return size;
}

int main(int argc, char* argv[]) {
#if defined(PLATFORM_WINDOWS) && defined(_DEBUG)
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
	//_CrtSetBreakAlloc(216);
#endif

	if (argc < 7) {
        printf("usage: test_delay_client <remote_ip> <remote_port> <duration second> <snd_interval_ms> <Test_Msg_size_min> <Test_Msg_size_max>");
		return -1;
	}

	char* remote_ip = argv[1];
	uint32_t remote_port = (uint32_t)atoi(argv[2]);
    uint64_t dur = (uint64_t)atoi(argv[3]);           //存活的时长
    uint64_t snd_interval = (uint64_t)atoi(argv[4]);  //发送的间隔
    uint32_t Test_Msg_size_min = (uint32_t)atoi(argv[5]);
    uint32_t Test_Msg_size_max = (uint32_t)atoi(argv[6]);

    if (Test_Msg_size_max < Test_Msg_size_min) {
        printf("Test_Msg_size_max must greater than Test_Msg_size_min\n");
		return -2;
	}

    if (Test_Msg_size_max > MAX_Test_Msg_SIZE) {
        printf("Test_Msg_size_max must less than 64kh\n");
		return -3;
	}

	kcpuv_t kcpuv = kcpuv_create();
	kcpuv_conv_t conv = kcpuv_connect(kcpuv, remote_ip, remote_port);
	uint64_t t = get_tick_ms();
	uint64_t nextSend = t + snd_interval;
	while (get_tick_ms() - t < dur) {
		kcpuv_run(kcpuv);

		uint64_t cur = get_tick_ms();
		if (cur > nextSend) {
            Test_Msg& Test_Msg = snd_Test_Msg_map[cur];
            memset(&Test_Msg, 0, sizeof(Test_Msg));
            int size = make_Test_Msg(cur, Test_Msg, Test_Msg_size_min, Test_Msg_size_max);
			if (size > 0) {
                snd_Test_Msg_map[Test_Msg.tick] = Test_Msg;
                kcpuv_send(kcpuv, conv, &Test_Msg, sizeof(uint64_t) + size);
				nextSend = cur + snd_interval;
			}
		}

        kcpuv_msg_t msg;
		uint64_t now = get_tick_ms();
		while (true) {
            int r = kcpuv_recv(kcpuv, &msg);
			if (r < 0) break;

            Test_Msg m;
			memset(&m, 0, sizeof(m));
            memcpy(&m, msg.data, msg.size < sizeof(m) ? msg.size : sizeof(m));

            std::map<uint64_t, Test_Msg>::iterator it = snd_Test_Msg_map.find(m.tick);
            if (it == snd_Test_Msg_map.end()) {
                printf("Test_Msg not found %" PRIu64 "\n", m.tick);
			} else {
				if (memcmp(&it->second, &m, sizeof(m)) == 0) {
                    printf("%" PRIu64 "\n", now - m.tick);
				} else {
                    printf("recved a diff Test_Msg %" PRIu64 "\n", m.tick);
				}
			}
		}

		sleep_ms(1);
	}

	kcpuv_destroy(kcpuv);
	return 0;
}
