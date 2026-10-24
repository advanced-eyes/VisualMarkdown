---
type: document
format: 1
id: 9a6c1f2e-3b4d-4e5f-8a7b-1c2d3e4f5a61
source: yolo.pdf
fileInfo: PDF 10쪽
meta: Redmon · Divvala · Girshick · Farhadi · CVPR 2016
learned: 2026-10-20T09:10:00+09:00
analysis: 0a1b2c3d-4e5f-4a6b-9c7d-8e9f0a1b2c31
rules: 1
edited: false
digest: 65919ee8c0dae918832f6d7a204122538d4ad62afcb2edd85efd101bb797e229
---

# You Only Look Once: Unified, Real-Time Object Detection

검출을 이미지 전체에 대한 한 번의 회귀 문제로 풀어, 실시간으로 상자와 클래스를 함께 맞힌다.

## 차례

- 1 소개
- 2 통합 검출
  - 2.1 네트워크 설계
  - 2.2 학습
  - 2.3 추론
  - 2.4 YOLO 의 한계
- 3 다른 검출 시스템과 비교
- 4 실험
- 5 실제 환경에서 실시간 검출
- 6 결론

## 핵심 숫자

- **63.4** mAP — VOC 2007 test
- **45** FPS — Titan X
- **155** FPS — Fast YOLO, 52.7 mAP

## 지식

- 검출을 이미지 전체에 대한 한 번의 회귀로 풀었다 <!-- card:a1b2c3d4 -->
  - 근거: 한 신경망이 상자와 클래스 확률을 한 번에 예측한다
  - 절: 2
  - 개념: [YOLO](../concepts/yolo.md), [객체 검출](../concepts/객체-검출.md)
- 실시간 검출기 가운데 가장 정확하다 <!-- card:b2c3d4e5 -->
  - 근거: VOC 2007 에서 63.4 mAP, 45 FPS
  - 절: 4
  - 개념: [YOLO](../concepts/yolo.md), [PASCAL VOC](../concepts/pascal-voc.md)
  - 메모: 표 1 의 실시간 검출기와 비교

## 관계

- [YOLO](../concepts/yolo.md) **해결** → [객체 검출](../concepts/객체-검출.md)
  - 위치: 1절
- [YOLO](../concepts/yolo.md) **대안** ↔ [Faster R-CNN](../concepts/faster-r-cnn.md)
  - 설명: 45 FPS 로 Faster R-CNN(7 FPS)보다 빠르지만 mAP 는 63.4 로 낮다
  - 위치: 표 1
  - 상태: AI 제안
- [Faster R-CNN](../concepts/faster-r-cnn.md) **해결** → [객체 검출](../concepts/객체-검출.md)
  - 위치: 3절
  - 상태: AI 제안

## 데이터

### 실시간 검출기와 그 밖의 검출기 — 정확도와 속도

<!-- ocean:data x="FPS" y="mAP" series="구분" concepts="yolo,faster-r-cnn,fast-r-cnn" condition="VOC 2007 test" where="표 1" -->

| 검출기 | 구분 | 학습 | mAP (%) | FPS |
| --- | --- | --- | --: | --: |
| 100Hz DPM | 실시간 | 2007 | 16.0 | 100 |
| 30Hz DPM | 실시간 | 2007 | 26.1 | 30 |
| Fast YOLO | 실시간 | 2007+2012 | 52.7 | 155 |
| YOLO | 실시간 | 2007+2012 | 63.4 | 45 |
| Fastest DPM | 실시간 아님 | 2007 | 30.4 | 15 |
| R-CNN Minus R | 실시간 아님 | 2007 | 53.5 | 6 |
| Fast R-CNN | 실시간 아님 | 2007+2012 | 70.0 | 0.5 |
| Faster R-CNN VGG-16 | 실시간 아님 | 2007+2012 | 73.2 | 7 |
| Faster R-CNN ZF | 실시간 아님 | 2007+2012 | 62.1 | 18 |
| YOLO VGG-16 | 실시간 아님 | 2007+2012 | 66.4 | 21 |
