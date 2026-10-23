---
type: document
format: 1
id: 3f1c2a9e-5b7d-4c8e-9a21-6d0e4b8f7c31
source: yolo9000.pdf
fileInfo: PDF 9쪽
meta: Redmon · Farhadi · CVPR 2017
learned: 2026-10-22T10:00:00+09:00
analysis: 8b2e6c1d-0a4f-4e7b-9c3a-1f5d2e8b7a60
rules: 1
edited: false
digest: d156f8f01468fcb00665df21d107446232e10c886db82306ef4ba96802e64205
---

# YOLO9000: Better, Faster, Stronger

YOLO 에 작은 개선을 쌓아 속도를 잃지 않고 정확도를 올리고, 9000개 넘는 클래스를 검출하게 했다.

## 차례

- 1 소개
- 2 더 좋게
  - 2.1 배치 정규화
- 3 더 빠르게

## 핵심 숫자

- **76.8** mAP — VOC 2007, 67 fps

## 지식

- 작은 개선을 쌓아 속도를 잃지 않고 정확도를 올렸다 <!-- card:7f3a2c1e -->
  - 근거: VOC 2007 mAP 63.4% → 76.8%, 67 fps
  - 절: 2
  - 개념: [YOLOv2](../concepts/yolov2.md)
- 입력 크기만 바꿔 속도와 정확도 사이를 고른다 <!-- card:2b9d04e6 -->
  - 근거: 288 px 에서 91 FPS · 69.0 mAP, 544 px 에서 40 FPS · 78.6 mAP
  - 절: 2
  - 개념: [YOLOv2](../concepts/yolov2.md)

## 관계

- [YOLOv2](../concepts/yolov2.md) **개선** → [YOLO](../concepts/yolo.md)
  - 설명: 배치 정규화 · anchor · 고해상도 학습으로 mAP 63.4 → 76.8, 속도 유지
  - 위치: 2절
  - 인용: "At 67 FPS, YOLOv2 gets 76.8 mAP on VOC 2007."
  - 상태: AI 제안
- [YOLOv2](../concepts/yolov2.md) **대안** ↔ [Faster R-CNN](../concepts/faster-r-cnn.md)
  - 설명: 40 FPS 에서 78.6 mAP 로 ResNet 을 쓴 Faster R-CNN 보다 정확하고 빠르다
  - 위치: 초록
  - 인용: "At 40 FPS, YOLOv2 gets 78.6 mAP, outperforming state-of-the-art methods like Faster RCNN with ResNet and SSD while still running significantly faster."
  - 상태: AI 제안
- [YOLOv2](../concepts/yolov2.md) **해결** → [객체 검출](../concepts/객체-검출.md)
  - 위치: 1절

## 데이터

### YOLO 에서 YOLOv2 까지 — 하나씩 더한 개선

<!-- ocean:data x="단계" y="mAP" ordered="단계" cumulative="yes" concepts="yolov2,yolo" condition="VOC 2007 test" where="표 2" -->

| 단계 | mAP (%) |
| --- | --: |
| YOLO (기준) | 63.4 |
| + 배치 정규화 | 65.8 |

### 속도와 정확도 — 입력 크기별

<!-- ocean:data x="FPS" y="mAP" series="검출기" concepts="yolov2" condition="VOC 2007 test · GTX Titan X" where="표 3" -->

| 검출기 | 입력 크기 (px) | mAP (%) | FPS |
| --- | --: | --: | --: |
| SSD | 300 | 74.3 | 46 |
| SSD | 500 | 76.8 | 19 |
| YOLOv2 | 288 | 69.0 | 91 |
| YOLOv2 | 352 | 73.7 | 81 |
| YOLOv2 | 416 | 76.8 | 67 |
| YOLOv2 | 480 | 77.8 | 59 |
| YOLOv2 | 544 | 78.6 | 40 |
