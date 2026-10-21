#define FEEDER_HOST_TEST
#include "../../src/button.cpp"

namespace {
uint32_t testNow = 0;
bool testDown = false;
bool testMotor = false;
uint32_t testSequence = 0;
unsigned starts = 0;
unsigned stops = 0;
unsigned accessPoints = 0;
uint32_t requestedDuration = 0;

void input(uint32_t now, bool down) {
    testNow = now;
    testDown = down;
    tickButton();
    tickButtonActions();
}
} // namespace

String errorText = "";

uint32_t millis() {
    return testNow;
}

int digitalRead(int) {
    return testDown ? LOW : HIGH;
}

void pinMode(int, int) {
}

bool startFeed(uint32_t duration) {
    if (testMotor) {
        return false;
    }
    testMotor = true;
    ++testSequence;
    ++starts;
    requestedDuration = duration;
    return true;
}

void stopFeed() {
    testMotor = false;
    ++stops;
}

void stopFeedIfSequence(uint32_t sequence) {
    if (sequence == testSequence) {
        stopFeed();
    }
}

bool motorIsFeeding() {
    return testMotor;
}

uint32_t motorFeedSequence() {
    return testSequence;
}

void startAP() {
    ++accessPoints;
}

#define CHECK(condition)                                                                           \
    if (!(condition)) {                                                                            \
        return __LINE__;                                                                           \
    }

int main() {
    beginButton();
    CHECK(!takeButtonDisplayActivity());
    // 1999ms松手不触发，进度复位后必须重新累计完整2秒。
    input(100, true);
    CHECK(takeButtonDisplayActivity() && !takeButtonDisplayActivity());
    input(140, true);
    CHECK(buttonIsArming());
    input(2099, true);
    CHECK(starts == 0);
    input(2100, false);
    CHECK(takeButtonDisplayActivity() && !takeButtonDisplayActivity());
    CHECK(!buttonIsArming() && starts == 0);
    input(2200, true);
    input(2240, true);
    input(4199, true);
    CHECK(starts == 0);
    input(4200, true);
    CHECK(buttonIsFeeding() && starts == 1 && requestedDuration == MAX_MOTOR_MS);
    input(4201, false);
    CHECK(!testMotor && stops == 1);

    // 达到安全时限后持续按住不重启；其他任务随后启动，松手不能误停它。
    input(5000, true);
    input(5040, true);
    input(7000, true);
    CHECK(starts == 2);
    testMotor = false;
    input(37000, true);
    CHECK(starts == 2 && !buttonIsFeeding());
    startFeed(1000);
    input(37001, false);
    CHECK(testMotor && stops == 1);
    testMotor = false;

    // 在未观察到旧任务结束的情况下也能通过序号识别新任务。
    input(38000, true);
    input(38040, true);
    input(40000, true);
    testMotor = false;
    startFeed(1000);
    input(40001, false);
    CHECK(testMotor && stops == 1);
    testMotor = false;

    // 上电按住只进入配网，松手不会投喂。
    testDown = true;
    beginButton();
    const unsigned previousStarts = starts;
    input(41000, true);
    input(41040, true);
    input(43000, true);
    CHECK(!buttonIsArming() && starts == previousStarts && accessPoints == 0);
    input(44000, true);
    CHECK(accessPoints == 1 && starts == previousStarts);
    input(44001, false);
    CHECK(starts == previousStarts);

    // 电机忙时按键不能接管已有投喂，松手也不能停止它。
    startFeed(1000);
    input(45000, true);
    input(45040, true);
    input(47000, true);
    CHECK(!buttonIsFeeding());
    input(47001, false);
    CHECK(testMotor && stops == 1);
    testMotor = false;
    // 双击只请求显示IP，不启动电机；请求消费一次后清空。
    input(50000, true);
    input(50040, true);
    input(50060, false);
    CHECK(!takeButtonIpRequest());
    input(50120, true);
    input(50160, true);
    input(50180, false);
    CHECK(takeButtonIpRequest());
    CHECK(!takeButtonIpRequest() && !testMotor);

    // 两次释放超过350ms不算双击；第二次改为长按也不产生双击事件。
    input(51000, true);
    input(51040, true);
    input(51060, false);
    input(51500, true);
    input(51540, true);
    input(51560, false);
    CHECK(!takeButtonIpRequest());
    input(51700, true);
    input(51740, true);
    input(53700, true);
    CHECK(buttonIsFeeding());
    input(53701, false);
    CHECK(!testMotor && !takeButtonIpRequest());

    // 毫秒计数溢出不影响2秒长按判断。
    input(0xFFFFFF00u, true);
    input(0xFFFFFF28u, true);
    input(0xFFFFFF00u + 1999u, true);
    CHECK(buttonIsArming());
    input(0xFFFFFF00u + 2000u, true);
    CHECK(buttonIsFeeding());
    input(0xFFFFFF00u + 2001u, false);
    CHECK(!testMotor);
    return 0;
}
