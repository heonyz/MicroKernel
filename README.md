# 마이크로 커널 프로젝트
<br>

## 프로젝트 동작 과정
1. **마이크로커널(`MicroKernel`) 생성**
   쓰레드 풀(`MicroKernelThreadPool`)을 준비하고, 마이크로커널이 사용할 플러그인 수 제한을 설정합니다.
   
2. **플러그인 등록**
   `plugin_register()`로 `BasicPlugin`, `AlarmPlugin` 플러그인을 등록합니다.
   이미 마이크로커널이 실행 중이면 등록 시점에 즉시 `plugin_init()`, `plugin_start()`가 호출되고, 정상 실행 상태가 됩니다.
   
3. **`micro_kernel->run()`**
   마이크로커널 메인 루프를 시작합니다.
   내부 루프에서 모든 플러그인에 대해 `plugin_task_en()`이 `true`인 경우 `plugin_task()`를 스레드 풀에 제출합니다.
   스레드 풀이 비동기로 플러그인의 `plugin_task()`를 실행해주기 때문에, 여러 플러그인이 동시에 동작할 수 있습니다.
   
4. **플러그인 간 메시지 통신**
   한 플러그인에서 다른 플러그인으로 메시지를 전송하고 싶으면 `message_dispatch(from, to, req, res)`를 호출합니다.
   마이크로커널은 `to`를 기준으로 해당 플러그인을 찾아 `plugin->message(req_msg, res_msg)`를 실행해주고, 결과를 `res`에 담아 돌려줍니다.
   
5. **마이크로커널 정지**
   마이크로커널의 실행을 중단하면, 루프를 빠져나와 `plugin_stop()`, `plugin_exit()` 등을 순차적으로 호출하여 각 플러그인을 정리합니다.
   쓰레드 풀도 모든 작업을 마친 뒤 종료됩니다.
