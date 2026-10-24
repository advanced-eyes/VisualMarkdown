---
type: document
format: 1
id: 5e2d8b10-7c4a-4f3e-b9d6-2a1f0c9e8d72
source: yolo9000.pdf
fileInfo: PDF 9쪽
meta: Redmon · Farhadi · CVPR 2017
learned: 2026-10-21T14:30:00+09:00
analysis: 1b2c3d4e-5f6a-4b7c-8d9e-9f0a1b2c3d42
rules: 1
edited: false
digest: d949281cb3461d5b23594d3796f2cf32f3fb62ba4de97386057a55e490937022
---

# YOLO9000: Better, Faster, Stronger

YOLO 에 작은 개선을 쌓아 속도를 잃지 않고 정확도를 올리고, 9000개 넘는 클래스를 검출하게 했다.

## 차례

- 1 소개
- 2 더 좋게
- 3 더 빠르게
- 4 더 강하게
- 5 결론

## 핵심 숫자

- **76.8** mAP — VOC 2007, 67 FPS
- **78.6** mAP — VOC 2007, 544×544 입력
- **9000** — 검출할 수 있는 클래스 수

## 지식

- 작은 개선을 쌓아 속도를 잃지 않고 정확도를 올렸다 <!-- card:c3d4e5f6 -->
  - 근거: VOC 2007 mAP 63.4% → 76.8%, 67 FPS
  - 절: 2
  - 개념: [YOLOv2](../concepts/yolov2.md), [YOLO](../concepts/yolo.md)
- 입력 크기만 바꿔 속도와 정확도 사이를 고른다 <!-- card:d4e5f6a7 -->
  - 근거: 288 px 에서 91 FPS · 69.0 mAP, 544 px 에서 40 FPS · 78.6 mAP
  - 절: 2
  - 개념: [YOLOv2](../concepts/yolov2.md)
- 새 바탕 네트워크 Darknet-19 로 계산을 줄였다 <!-- card:e5f6a7b8 -->
  - 근거: 이미지 하나에 55억 8천만 번 연산, ImageNet top-1 72.9%
  - 절: 3
  - 개념: [Darknet-19](../concepts/darknet-19.md)

## 관계

- [YOLOv2](../concepts/yolov2.md) **개선** → [YOLO](../concepts/yolo.md)
  - 설명: 배치 정규화 · anchor · 고해상도 학습으로 mAP 63.4 → 76.8, 속도 유지
  - 위치: 2절
  - 인용: "At 67 FPS, YOLOv2 gets 76.8 mAP on VOC 2007."
  - 상태: AI 제안
- [YOLOv2](../concepts/yolov2.md) **사용** → [Darknet-19](../concepts/darknet-19.md)
  - 위치: 3절
- [YOLOv2](../concepts/yolov2.md) **사용** → [배치 정규화](../concepts/배치-정규화.md)
  - 설명: 모든 합성곱 층에 배치 정규화를 붙여 mAP 를 2% 넘게 올렸다
  - 위치: 2절
  - 인용: "By adding batch normalization on all of the convolutional layers in YOLO we get more than 2% improvement in mAP."
  - 상태: AI 제안
- [YOLOv2](../concepts/yolov2.md) **대안** ↔ [SSD](../concepts/ssd.md)
  - 설명: VOC 2007 에서 SSD500(76.8 mAP, 19 FPS)과 정확도가 같고 더 빠르다(67 FPS)
  - 위치: 표 3
  - 상태: AI 제안

## 데이터

### YOLO 에서 YOLOv2 까지 — 하나씩 더한 개선

<!-- ocean:data x="단계" y="mAP" ordered="단계" cumulative="yes" concepts="yolov2,yolo" condition="VOC 2007 test" where="표 2" -->

| 단계 | mAP (%) |
| --- | --: |
| YOLO (기준) | 63.4 |
| + 배치 정규화 | 65.8 |
| + 고해상도 분류기 | 69.5 |
| + 합성곱 · anchor 상자 | 69.2 |
| + 새 네트워크 | 69.6 |
| + 차원 사전 · 위치 예측 | 74.4 |
| + passthrough | 75.4 |
| + 다중 크기 학습 | 76.8 |
| + 고해상도 검출기 | 78.6 |

### 입력 크기에 따른 정확도와 속도

<!-- ocean:data x="입력 크기" y="mAP,FPS" ordered="입력 크기" concepts="yolov2" condition="VOC 2007 test · GTX Titan X" where="표 3" -->

| 입력 크기 (px) | mAP (%) | FPS |
| --- | --: | --: |
| 288 | 69.0 | 91 |
| 352 | 73.7 | 81 |
| 416 | 76.8 | 67 |
| 480 | 77.8 | 59 |
| 544 | 78.6 | 40 |

### 검출기별 VOC 2007 정확도

<!-- ocean:data x="검출기" y="mAP" concepts="fast-r-cnn,faster-r-cnn,yolo,ssd,yolov2" condition="VOC 2007 test" where="표 3" -->

| 검출기 | mAP (%) |
| --- | --: |
| Fast R-CNN | 70.0 |
| Faster R-CNN VGG-16 | 73.2 |
| Faster R-CNN ResNet | 76.4 |
| YOLO | 63.4 |
| SSD300 | 74.3 |
| SSD500 | 76.8 |
| YOLOv2 544 | 78.6 |
