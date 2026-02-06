<div contenteditable="true" translate="no" class="ProseMirror"><h1>EduDot AI Core Engine</h1><p><strong>EduDot AI Core</strong>는 시각장애 학생의 학습권 보장을 위해 학습 자료(이미지)를 **표준 점자 파일(.brf)**로 자동 변환하는 고성능 멀티모달 파이프라인입니다.</p><h2>1. 프로젝트 개요</h2><p>이 저장소는 <strong>GCP AI Computing Server</strong>에서 구동되는 핵심 추론 엔진을 포함합니다. OCR, VLM, LLM 기술을 유기적으로 연결하여 복잡한 수식과 그래프가 포함된 이공계 학습 자료를 해석하고, <strong>2024년 개정 한국 점자 규정</strong>에 맞춰 변환합니다.</p><h3>🚩 핵심 목표</h3><ul><li><p><strong>Zero-Lag Learning</strong>: 촬영 즉시 점역 결과 제공.</p></li><li><p><strong>High Fidelity</strong>: 수식(LaTeX) 및 도표(Caption)의 정확한 구조화.</p></li><li><p><strong>Standard Compliance</strong>: 국립국어원 점자 규정 및 점자정보단말기 규격(BRF) 준수.</p></li></ul><h2>2. 아키텍처 및 기술 스택</h2><p>본 프로젝트는 <strong>NVIDIA GPU(T4/L4)</strong> 환경에 최적화된 <strong>Cascade Inference</strong> 아키텍처를 채택했습니다.</p><h3>🧩 AI 모델 파이프라인</h3><p>|</p><p>| <strong>단계</strong> | <strong>역할</strong> | <strong>모델 / 라이브러리</strong> | <strong>특징</strong> |
| <strong>PART 2</strong> | 페이지 탐지 | <strong>Florence-2-base</strong> | Corner Merging 전략으로 정확도 향상 |
| <strong>PART 3</strong> | 문서 OCR | <strong>DeepSeek-OCR-2</strong> | 고해상도 Crop Mode 추론, Markdown 출력 |
| <strong>PART 4</strong> | 이미지 캡셔닝 | <strong>Qwen2-VL-7B</strong> | Int4 양자화, 차트/그래프 해석 |
| <strong>PART 5</strong> | 캡션 최적화 | <strong>GPT-4o / Gemini</strong> | 점역용 문체 정제 (Rule-based Prompting) |
| <strong>PART 6</strong> | 점역 엔진 | <strong>Liblouis</strong> | 한국어 1급 점자 테이블(<code>ko-g1.ctb</code>) 커스텀 |
| <strong>PART 7</strong> | 파일 생성 | <strong>Custom Logic</strong> | BRF 포맷팅 및 HTML 미리보기 생성 |</p><h3>🛠 시스템 환경</h3><ul><li><p><strong>OS</strong>: Ubuntu Linux (CUDA 12.1 지원 필수)</p></li><li><p><strong>Language</strong>: Python 3.10</p></li><li><p><strong>Web Framework</strong>: FastAPI, Uvicorn</p></li><li><p><strong>Serving Strategy</strong>: High-Performance Mode (모델 메모리 상주)</p></li></ul><h2>3. 설치 및 환경 설정</h2><blockquote><p><strong>⚠️ 중요</strong>: 본 프로젝트는 모델 호환성을 위해 <code>transformers==4.46.3</code> 버전을 엄격히 요구합니다.</p></blockquote><h3>3.1. 사전 준비</h3><ul><li><p>NVIDIA Driver 설치 (CUDA 12.1 이상 호환)</p></li><li><p><code>sudo</code> 권한 (시스템 패키지 설치용)</p></li></ul><h3>3.2. 자동 설치 (Golden Set)</h3><p>프로젝트 루트의 <code>setup.sh</code>를 실행하여 가상환경 구성, 시스템 라이브러리(Liblouis, FFmpeg), 그리고 검증된 버전의 Python 패키지를 한 번에 설치합니다.</p><pre><code># 1. 실행 권한 부여
chmod +x setup.sh

# 2. 설치 스크립트 실행
./setup.sh

# 3. 가상환경 활성화
source venv/bin/activate

<br class="ProseMirror-trailingBreak"></code></pre><h3>3.3. 환경 변수 설정</h3><p><code>.env</code> 파일을 생성하고 다음 키를 설정해야 합니다.</p><pre><code># .env 예시
OPENAI_API_KEY=sk-proj-...
GEMINI_API_KEY=AIzaSy...

# 모델 경로 (필요시 수정)
MODEL_OCR_PATH=deepseek-ai/DeepSeek-OCR-2
MODEL_CAPTION_PATH=Qwen/Qwen2-VL-7B-Instruct

# 디버깅 옵션
CLEANUP_TEMP=False

<br class="ProseMirror-trailingBreak"></code></pre><h2>4. 디렉토리 구조</h2><pre><code>AI/
├── .env                          # [설정] 환경 변수
├── setup.sh                      # [실행] 환경 설정 자동화 스크립트
├── app/                          # [소스] 메인 애플리케이션
│   ├── config.py                 # [설정] 전역 상수 및 경로
│   ├── main.py                   # [실행] FastAPI 진입점
│   ├── ai/                       # [AI] 핵심 엔진 모듈
│   │   ├── braille_generator.py  # Part 7: 파일 생성기
│   │   ├── braille_optimizer.py  # Part 5: 캡션 최적화 (LLM)
│   │   ├── braille_translator.py # Part 6: 점역기 (Liblouis)
│   │   ├── caption_engine.py     # Part 4: VLM 캡셔닝
│   │   ├── model_manager.py      # 리소스 관리자 (Singleton)
│   │   ├── ocr_engine.py         # Part 3: DeepSeek OCR
│   │   └── page_detector.py      # Part 2: 페이지 탐지
│   ├── api/routes.py             # [API] 엔드포인트 정의
│   ├── services/                 # [서비스] 비즈니스 로직
│   │   ├── job_manager.py        # 작업 수명주기 관리
│   │   └── pipeline.py           # 전체 파이프라인 오케스트레이터
│   └── utils/                    # [유틸] 공통 도구
├── runtime/                      # [데이터] 실행 데이터 저장소
│   └── jobs/                     # 작업별 격리 공간 (input/output/temp)
└── test/                         # [테스트] 테스트 스위트
    ├── local_runner.py           # 로컬 통합 테스트 실행기
    └── unit_test/                # 파트별 단위 테스트

<br class="ProseMirror-trailingBreak"></code></pre><h2>5. 실행 및 사용 방법</h2><h3>5.1. 로컬 통합 테스트 (Local Runner)</h3><p>서버를 띄우지 않고 <code>images/</code> 폴더에 있는 이미지들을 사용하여 전체 파이프라인을 검증합니다.</p><ol><li><p>프로젝트 루트에 <code>images/</code> 폴더 생성 및 테스트 이미지(<code>1.jpg</code> 등) 넣기.</p></li><li><p>실행:</p><pre><code>python test/local_runner.py

<br class="ProseMirror-trailingBreak"></code></pre></li><li><p>결과는 <code>runtime/jobs/local_test_{timestamp}/output/</code> 에서 확인.</p></li></ol><h3>5.2. API 서버 실행</h3><p>FastAPI 서버를 구동하여 외부 요청을 처리합니다.</p><pre><code># 개발 모드 (Auto-reload)
python app/main.py

# 또는 Uvicorn 직접 실행
uvicorn app.main:app --host 0.0.0.0 --port 8000

<br class="ProseMirror-trailingBreak"></code></pre><h3>5.3. 주요 API 엔드포인트</h3><ul><li><p><code>POST /job/start</code>: 작업 세션 초기화.</p></li><li><p><code>POST /job/image</code>: 이미지 업로드 및 즉시 처리 (Streaming Pipeline).</p></li><li><p><code>POST /job/finish</code>: 최종 BRF 파일 병합 및 반환.</p></li><li><p><code>GET /</code>: Health Check.</p></li></ul><h2>6. 단위 테스트 (Unit Tests)</h2><p>각 모듈의 독립성을 검증하기 위해 파트별 단위 테스트가 준비되어 있습니다.</p><p>| <strong>파트</strong> | <strong>테스트 파일</strong> | <strong>설명</strong> | <strong>주요 검증 항목</strong> |
| <strong>Part 1</strong> | <code>test_part1.py</code> | 이미지 전처리 | OpenCV 필터 동작 여부 |
| <strong>Part 2</strong> | <code>test_part2.py</code> | 페이지 탐지 | Florence-2 숫자 추출 정확도 |
| <strong>Part 3</strong> | <code>test_part3.py</code> | 문서 OCR | DeepSeek <code>.mmd</code> 생성 및 Crop 저장 |
| <strong>Part 4</strong> | <code>test_part4.py</code> | 캡셔닝 | Qwen2-VL 캡션 생성 (Mock Input) |
| <strong>Part 5</strong> | <code>test_part5.py</code> | 최적화 | LLM(OpenAI) 응답 및 텍스트 정제 |
| <strong>Part 6</strong> | <code>test_part6.py</code> | 점역 | Liblouis 전처리 및 유니코드 변환 |
| <strong>Part 7</strong> | <code>test_part7.py</code> | 파일 생성 | BRF 포맷팅 및 HTML 태그 구조 |</p><p><strong>테스트 실행 예시:</strong></p><pre><code># Part 3 (OCR) 테스트 실행
python -m test.unit_test.part3.test_part3

<br class="ProseMirror-trailingBreak"></code></pre></div>