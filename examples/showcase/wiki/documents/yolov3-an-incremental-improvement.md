---
type: document
format: 1
id: 7b3e9c21-4d5f-4a6b-8c7d-3e2f1a0b9c83
source: yolov3.pdf
fileInfo: PDF 6쪽
meta: Redmon · Farhadi · arXiv 2018
learned: 2026-10-22T11:05:00+09:00
analysis: 2c3d4e5f-6a7b-4c8d-9e0f-0a1b2c3d4e53
rules: 1
edited: false
digest: 1a1ae3eab8424a458e95087e22532283f1406bce15c2107eadce702642cdb8a3
---

# YOLOv3: An Incremental Improvement

YOLO 에 작은 설계 변화를 여럿 더해, 조금 커졌지만 더 정확해졌고 여전히 빠르다.

## 차례

- 1 소개
- 2 바뀐 것
  - 2.1 상자 예측
  - 2.2 클래스 예측
  - 2.3 여러 크기에서 예측
  - 2.4 특징 추출기
  - 2.5 학습
- 3 결과
- 4 해 봤지만 안 된 것
- 5 의미

## 핵심 숫자

- **57.9** AP50 — COCO, 608×608, 51 ms
- **22** ms — 320×320, 28.2 mAP

## 지식

- 정확도는 RetinaNet 과 비슷하지만 3.8배 빠르다 <!-- card:f6a7b8c9 -->
  - 근거: Titan X 에서 51 ms 에 57.9 AP50, RetinaNet 은 198 ms 에 57.5 AP50
  - 절: 3
  - 개념: [YOLOv3](../concepts/yolov3.md), [RetinaNet](../concepts/retinanet.md)
- 세 크기의 특징 지도에서 상자를 예측한다 <!-- card:a7b8c9d0 -->
  - 근거: 크기마다 상자 3개, 모두 9개의 사전 상자
  - 절: 2.3
  - 개념: [YOLOv3](../concepts/yolov3.md)

## 관계

- [YOLOv3](../concepts/yolov3.md) **개선** → [YOLOv2](../concepts/yolov2.md)
  - 설명: 여러 크기에서 예측하고 바탕 네트워크를 키워 작은 물체를 더 잘 찾는다
  - 위치: 2절
  - 상태: AI 제안
- [YOLOv2](../concepts/yolov2.md) **개선** → [YOLO](../concepts/yolo.md)
- [YOLOv3](../concepts/yolov3.md) **사용** → [Darknet-53](../concepts/darknet-53.md)
  - 설명: 합성곱 53층으로 된 새 바탕 네트워크
  - 위치: 2.4절
  - 상태: AI 제안
- [YOLOv3](../concepts/yolov3.md) **대안** ↔ [RetinaNet](../concepts/retinanet.md)
  - 설명: 57.9 AP50 로 RetinaNet(57.5)과 비슷하고 3.8배 빠르다
  - 위치: 초록
  - 인용: "It achieves 57.9 AP50 in 51 ms on a Titan X, compared to 57.5 AP50 in 198 ms by RetinaNet, similar performance but 3.8× faster."
  - 상태: AI 제안

## 데이터

### 입력 크기별 COCO 정확도

<!-- ocean:data x="입력 크기" y="AP,AP50" ordered="입력 크기" concepts="yolov3,coco" condition="COCO test-dev" where="그림 1 · 그림 3" -->

| 입력 크기 (px) | AP (%) | AP50 (%) |
| --- | --: | --: |
| 320 | 28.2 | 51.5 |
| 416 | 31.0 | 55.3 |
| 608 | 33.0 | 57.9 |

### 처리 시간과 AP50

<!-- ocean:data x="시간" y="AP50" concepts="yolov3" condition="COCO test-dev · Titan X" where="그림 3" -->

| 입력 크기 (px) | 시간 (ms) | AP50 (%) |
| --: | --: | --: |
| 320 | 22 | 51.5 |
| 416 | 29 | 55.3 |
| 608 | 51 | 57.9 |

### 검출기별 COCO 정확도

<!-- ocean:data x="검출기" y="AP,AP50,AP75,APS,APM,APL" concepts="yolov3,retinanet,yolov2,coco" condition="COCO test-dev" where="표 3" -->

| 검출기 | 바탕 네트워크 | AP (%) | AP50 (%) | AP75 (%) | APS (%) | APM (%) | APL (%) |
| --- | --- | --: | --: | --: | --: | --: | --: |
| Faster R-CNN+++ | ResNet-101-C4 | 34.9 | 55.7 | 37.4 | 15.6 | 38.7 | 50.9 |
| Faster R-CNN w FPN | ResNet-101-FPN | 36.2 | 59.1 | 39.0 | 18.2 | 39.0 | 48.2 |
| Faster R-CNN by G-RMI | Inception-ResNet-v2 | 34.7 | 55.5 | 36.7 | 13.5 | 38.1 | 52.0 |
| Faster R-CNN w TDM | Inception-ResNet-v2-TDM | 36.8 | 57.7 | 39.2 | 16.2 | 39.8 | 52.1 |
| YOLOv2 | DarkNet-19 | 21.6 | 44.0 | 19.2 | 5.0 | 22.4 | 35.5 |
| SSD513 | ResNet-101-SSD | 31.2 | 50.4 | 33.3 | 10.2 | 34.5 | 49.8 |
| DSSD513 | ResNet-101-DSSD | 33.2 | 53.3 | 35.2 | 13.0 | 35.4 | 51.1 |
| RetinaNet R-101 | ResNet-101-FPN | 39.1 | 59.1 | 42.3 | 21.8 | 42.7 | 50.2 |
| RetinaNet X-101 | ResNeXt-101-FPN | 40.8 | 61.1 | 44.1 | 24.1 | 44.2 | 51.2 |
| YOLOv3 608×608 | Darknet-53 | 33.0 | 57.9 | 34.4 | 18.3 | 35.4 | 41.9 |
