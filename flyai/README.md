<div contenteditable="true" translate="no" class="ProseMirror" bis_skin_checked="1"><h1>EduDot AI Core Engine</h1><p><strong>EduDot AI Core</strong>는 시각장애 학생의 교육 평등을 실현하기 위해 개발된 고성능 <strong>이미지-점자 변환(Image-to-Braille)</strong> 파이프라인입니다.
복잡한 이공계 학습 자료(수식, 그래프, 도표)를 인식하여 <strong>2024년 개정 한국 점자 규정</strong>에 부합하는 **표준 점자 파일(.brf)**을 실시간으로 생성합니다.</p><h2>1. 프로젝트 개요 (Overview)</h2><p>이 저장소는 <strong>NVIDIA GPU Server</strong>에서 구동되는 핵심 추론 엔진(Inference Engine)을 포함합니다. OCR, VLM(Vision Language Model), LLM을 유기적으로 연결한 <strong>Cascade Architecture</strong>를 통해 시각 정보를 텍스트로 변환하고, 이를 다시 점자 코드로 번역합니다.</p><h3>🚩 핵심 목표</h3><ul><li><p><strong>Zero-Lag Learning</strong>: 촬영과 동시에 점역 결과를 제공하는 초고속 파이프라인.</p></li><li><p><strong>High Fidelity</strong>: LaTeX 수식 및 복잡한 도표(Chart/Graph)의 맥락을 완벽하게 구조화.</p></li><li><p><strong>Standard Compliance</strong>: 국립국어원 점자 규정 및 점자정보단말기 규격(BRF) 완벽 준수.</p></li></ul><h2>2. 아키텍처 및 파이프라인 (Architecture)</h2><p>본 프로젝트는 모듈화된 7단계 파이프라인으로 구성되어 있으며, 각 단계는 독립적인 엔진에 의해 처리됩니다.</p><table><tbody><tr><th><p>단계</p></th><th><p>모듈 파일 (<code>app/ai/</code>)</p></th><th><p>역할</p></th><th><p>주요 기술/모델</p></th></tr><tr><td><p><strong>Part 1</strong></p></td><td><p><code>page_detector.py</code></p></td><td><p>페이지 탐지</p></td><td><p>문서 영역 감지 및 Perspective Crop</p></td></tr><tr><td><p><strong>Part 2</strong></p></td><td><p><code>ocr_engine.py</code></p></td><td><p>텍스트 추출</p></td><td><p>다국어/수식 인식 (DeepSeek-OCR 기반)</p></td></tr><tr><td><p><strong>Part 2.5</strong></p></td><td><p><code>caption_engine.py</code></p></td><td><p>이미지 캡셔닝</p></td><td><p>도표/그래프 해석 (Qwen2-VL)</p></td></tr><tr><td><p><strong>Part 5</strong></p></td><td><p><code>braille_optimizer.py</code></p></td><td><p>점역 최적화</p></td><td><p>LLM을 이용한 점자용 문체 교정 및 구조화</p></td></tr><tr><td><p><strong>Part 6</strong></p></td><td><p><code>braille_translator.py</code></p></td><td><p>점역 엔진</p></td><td><p>Liblouis 기반 한글 점자 변환 (Unicode Braille)</p></td></tr><tr><td><p><strong>Part 7</strong></p></td><td><p><code>braille_generator.py</code></p></td><td><p>파일 생성</p></td><td><p>BRF 포맷팅 및 미리보기 HTML 생성</p></td></tr></tbody></table><blockquote><p><strong>Note</strong>: <code>model_manager.py</code>가 위 모델들의 로드 및 메모리 관리를 총괄하며, <code>services/pipeline.py</code>가 전체 워크플로우를 조율합니다.</p></blockquote><h2>3. 설치 및 환경 설정 (Setup)</h2><p>이 프로젝트는 모델 호환성을 위해 <strong>Python 3.10</strong> 및 <strong>CUDA 12.1</strong> 환경을 권장합니다.</p><h3>3.1. 사전 요구사항</h3><ul><li><p>NVIDIA Driver (CUDA 12.1 이상 호환)</p></li><li><p><code>sudo</code> 권한 (Liblouis 시스템 라이브러리 설치용)</p></li></ul><h3>3.2. 자동 설치 (Recommended)</h3><p>프로젝트 루트의 <code>setup.sh</code> 스크립트를 통해 가상환경 생성, 필수 시스템 패키지 설치, Python 라이브러리 설치를 한 번에 수행합니다.</p><pre><code># 1. 실행 권한 부여
chmod +x setup.sh

# 2. 설치 스크립트 실행 (약 5~10분 소요)
./setup.sh

# 3. 가상환경 활성화
source venv/bin/activate
<br class="ProseMirror-trailingBreak"></code></pre><h3>3.3. 환경 변수 설정 (.env)</h3><p>프로젝트 루트에 <code>.env</code> 파일을 생성하고 아래 API Key들을 설정해야 합니다.</p><pre><code># AI Model APIs
OPENAI_API_KEY=sk-proj-...
GEMINI_API_KEY=AIzaSy...

# System Configuration
MAX_WORKERS=4
LOG_LEVEL=INFO
<br class="ProseMirror-trailingBreak"></code></pre><h2>4. 디렉토리 구조 (Directory Structure)</h2><p>프로젝트는 명확한 역할 분리를 위해 다음과 같은 구조로 조직되어 있습니다.</p><pre><code>AI/
├── .env                          # [설정] API Key 등 민감한 환경 변수
├── setup.sh                      # [실행] 환경 설정 자동화 스크립트
├── requirements.txt              # [설정] 프로젝트 의존성 패키지 목록
├── README.md                     # [문서] 프로젝트 설명 및 가이드
├── images/                       # [리소스] 로컬 테스트용 원본 이미지 (1.jpg ~ 4.jpg)
├── app/                          # [소스] 앱 메인 디렉토리
│   ├── config.py                 # [설정] 앱 전체 환경 상수
│   ├── main.py                   # [실행] FastAPI 서버 진입점
│   ├── ai/                       # [AI] 핵심 AI 엔진 모음
│   │   ├── braille_generator.py  # [Part 7] BRF/HTML 생성
│   │   ├── braille_optimizer.py  # [Part 5] LLM 최적화
│   │   ├── braille_translator.py # [Part 6] 텍스트-점자 변환
│   │   ├── caption_engine.py     # [Part 2.5] 이미지 캡셔닝
│   │   ├── model_manager.py      # [관리] 모델 리소스 관리
│   │   ├── ocr_engine.py         # [Part 2] OCR 엔진
│   │   └── page_detector.py      # [Part 1] 페이지 감지
│   ├── api/                      # [백엔드] API 라우팅 (routes.py)
│   ├── services/                 # [백엔드] 비즈니스 로직
│   │   ├── job_manager.py        # Job 상태 관리
│   │   └── pipeline.py           # 파이프라인 조율자
│   └── utils/                    # [유틸] 공통 도구 (텍스트, 경로, 파일)
├── runtime/                      # [데이터] 실행 데이터 저장소
│   └── jobs/                     # Job 단위 결과 (input, output, temp)
└── test/                         # [테스트] 테스트 코드 및 도구
    ├── back_translator.py        # 점자 역점역 검증 도구
    ├── local_runner.py           # 전체 파이프라인 로컬 실행기
    └── unit_test/                # 파트별 단위 테스트 폴더 (part1 ~ part7)
<br class="ProseMirror-trailingBreak"></code></pre><h2>5. 실행 및 사용 방법 (Usage)</h2><h3>5.1. 로컬 통합 테스트 (Local Runner)</h3><p>서버를 띄우지 않고 <code>images/</code> 폴더의 이미지를 사용하여 전체 파이프라인을 검증할 수 있습니다.</p><pre><code># 로컬 러너 실행
python test/local_runner.py
<br class="ProseMirror-trailingBreak"></code></pre><ul><li><p><strong>입력</strong>: <code>images/</code> 폴더 내의 이미지 파일들.</p></li><li><p><strong>출력</strong>: <code>runtime/jobs/local_test_{timestamp}/</code> 경로에 결과 생성.</p></li></ul><h3>5.2. API 서버 구동</h3><p>FastAPI 서버를 실행하여 외부 요청을 처리합니다.</p><pre><code># Uvicorn을 이용한 서버 실행
uvicorn app.main:app --host 0.0.0.0 --port 8000 --reload
<br class="ProseMirror-trailingBreak"></code></pre><ul><li><p><strong>API Docs</strong>: <code>http://localhost:8000/docs</code> 접속 시 Swagger UI 확인 가능.</p></li></ul><h3>5.3. 주요 API 흐름</h3><ol><li><p><code>POST /job/start</code>: 새로운 작업 세션 생성 (Job ID 발급).</p></li><li><p><code>POST /job/image</code>: 이미지 업로드 및 즉시 분석 (Streaming).</p></li><li><p><code>POST /job/finish</code>: 최종 BRF 다운로드.</p></li></ol><h2>6. 단위 테스트 (Unit Tests)</h2><p>각 모듈의 안정성을 검증하기 위해 파트별 단위 테스트를 제공합니다. 테스트는 <code>test/unit_test</code> 폴더 내에 구성되어 있습니다.</p><h3>테스트 실행 방법</h3><p>프로젝트 루트에서 다음 명령어를 사용하여 특정 파트를 테스트할 수 있습니다.</p><table><tbody><tr><th><p>파트</p></th><th><p>테스트 목적</p></th><th><p>실행 명령어</p></th></tr><tr><td><p><strong>Part 1</strong></p></td><td><p>페이지 감지 및 크롭 성능 검증</p></td><td><p><code>python -m test.unit_test.part1.test_part1</code></p></td></tr><tr><td><p><strong>Part 2</strong></p></td><td><p>OCR 텍스트 추출 정확도 검증</p></td><td><p><code>python -m test.unit_test.part2.test_part2</code></p></td></tr><tr><td><p><strong>Part 3</strong></p></td><td><p>(Deprecated) 레거시 OCR 모듈</p></td><td><p><code>python -m test.unit_test.part3.test_part3</code></p></td></tr><tr><td><p><strong>Part 4</strong></p></td><td><p>캡션 생성 엔진 연동 테스트</p></td><td><p><code>python -m test.unit_test.part4.test_part4</code></p></td></tr><tr><td><p><strong>Part 5</strong></p></td><td><p>LLM 기반 텍스트 최적화 검증</p></td><td><p><code>python -m test.unit_test.part5.test_part5</code></p></td></tr><tr><td><p><strong>Part 6</strong></p></td><td><p>Liblouis 점역 변환 정확도 검증</p></td><td><p><code>python -m test.unit_test.part6.test_part6</code></p></td></tr><tr><td><p><strong>Part 7</strong></p></td><td><p>BRF 및 HTML 파일 생성 검증</p></td><td><p><code>python -m test.unit_test.part7.test_part7</code></p></td></tr></tbody></table><blockquote><p><strong>Tip</strong>: 각 테스트 실행 후 결과물은 <code>test/unit_test/partX/output/</code> 폴더에 저장됩니다.</p></blockquote><h2>7. 기여 (Contributing)</h2><p>EduDot AI 팀은 여러분의 기여를 환영합니다.</p><ol><li><p>이 저장소를 <strong>Fork</strong> 하세요.</p></li><li><p>새로운 기능 브랜치를 생성하세요 (<code>git checkout -b feature/AmazingFeature</code>).</p></li><li><p>변경 사항을 <strong>Commit</strong> 하세요 (<code>git commit -m 'Add AmazingFeature'</code>).</p></li><li><p>브랜치에 <strong>Push</strong> 하세요 (<code>git push origin feature/AmazingFeature</code>).</p></li><li><p><strong>Pull Request</strong>를 생성해주세요.</p></li></ol><p>&lt;div align="center"&gt;
&lt;sub&gt;Built with ❤️ by Team EduDot. Empowering education for everyone.&lt;/sub&gt;
&lt;/div&gt;</p></div>
