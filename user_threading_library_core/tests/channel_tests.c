#include "types.h"
#include "user.h"
#include "uthreads.h"

static void fail(const char *msg) {
  printf(1, "channel_tests: FAIL: %s\n", msg);
  exit();
}

/* Test 1: receiver blocks on empty channel, then send wakes it with correct
 * payload */
static channel_t *t1_ch;
static void *t1_receiver(void *arg) {
  void *data = 0;
  // Should block here until main thread sends data
  int rc = channel_recv(t1_ch, &data);
  if (rc != 0 || (int)data != 42) {
    fail("basic send/recv mismatch");
  }
  return 0;
}

static void test_basic_send_recv(void) {
  t1_ch = channel_create(2);
  if (!t1_ch)
    fail("channel_create failed");

  int rtid = thread_create(t1_receiver, 0);
  if (rtid < 0)
    fail("receiver thread create");

  // Receiver should block until we send.
  if (channel_send(t1_ch, (void *)42) != 0) {
    fail("basic send failed");
  }
  thread_join(rtid);
}

/* Test 2: sender blocks when channel is full and resumes once a recv frees
 * space */
static channel_t *t2_ch;
static volatile int t2_sender_returned = 0;
static volatile int t2_sender_result = -2;

static void *t2_sender(void *arg) {
  // This send should block if the channel is already full
  t2_sender_result = channel_send(t2_ch, (void *)2);
  t2_sender_returned = 1;
  return 0;
}

static void test_block_on_full_then_recv(void) {
  t2_ch = channel_create(1); // Capacity 1
  if (!t2_ch)
    fail("channel_create failed");

  // Fill the channel. Now count = 1, capacity = 1.
  if (channel_send(t2_ch, (void *)1) != 0)
    fail("initial send failed");

  int stid = thread_create(t2_sender, 0);
  if (stid < 0)
    fail("sender thread create");

  // Give sender a chance to block on full buffer.
  thread_yield();
  if (t2_sender_returned) {
    fail("sender should have blocked on full channel");
  }

  // Receive one item to free space; this should unblock sender.
  void *got = 0;
  if (channel_recv(t2_ch, &got) != 0 || (int)got != 1) {
    fail("recv from full buffer failed");
  }

  thread_join(stid);
  if (!t2_sender_returned || t2_sender_result != 0) {
    fail("sender did not complete after space freed");
  }

  // Now the sender's value (2) should be available.
  if (channel_recv(t2_ch, &got) != 0 || (int)got != 2) {
    fail("second recv got wrong data");
  }
}

/* Test 3: closing wakes a blocked receiver and rejects future sends/recvs */
static channel_t *t3_ch;
static volatile int t3_recv_result = -2;

static void *t3_waiter(void *arg) {
  void *data = 0;
  // should block, then wake up returning -1 when channel is closed
  t3_recv_result = channel_recv(t3_ch, &data);
  return 0;
}

static void test_close_behavior(void) {
  t3_ch = channel_create(1);
  if (!t3_ch)
    fail("channel_create failed");

  // Start a receiver that will block on empty channel.
  int rtid = thread_create(t3_waiter, 0);
  if (rtid < 0)
    fail("waiter thread create");

  thread_yield(); // let waiter block

  // Closing the channel should wake up the waiter with an error
  channel_close(t3_ch);
  thread_join(rtid);
  if (t3_recv_result != -1) {
    fail("receiver should return -1 after close on empty channel");
  }

  // Sends after close should fail.
  if (channel_send(t3_ch, (void *)5) != -1) {
    fail("send after close should fail");
  }

  // Recv after close with empty buffer should fail.
  void *data = 0;
  if (channel_recv(t3_ch, &data) != -1) {
    fail("recv after close should fail when empty");
  }
}

int main(void) {
  thread_init();

  test_basic_send_recv();
  test_block_on_full_then_recv();
  test_close_behavior();

  printf(1, "channel_tests: PASS\n");
  exit();
}
