---
type: document
format: 1
id: 2d5a8b43-9f0c-4d1e-b2f3-a4c5d6e7f805
source: focal-loss.pdf
fileInfo: PDF 10쪽
meta: Lin · Goyal · Girshick · He · Dollár · ICCV 2017
learned: 2026-10-23T10:15:00+09:00
analysis: 3d4e5f6a-7b8c-4d9e-8f0a-1b2c3d4e5f64
rules: 1
edited: false
digest: 0dd764a5dd61313f8db219f01debf082acee5d57fade788a3e93aeb8575fdf1f
---

# Focal Loss for Dense Object Detection

1단계 검출기가 2단계보다 덜 정확한 까닭을 쉬운 배경 예가 너무 많은 클래스 불균형으로 보고, 이를 줄이는 Focal Loss 를 제안했다.

## 차례

- 1 소개
- 2 관련 연구
- 3 Focal Loss
- 4 RetinaNet 검출기
- 5 실험
- 6 결론

## 핵심 숫자

- **39.1** AP — COCO test-dev, ResNet-101-FPN

## 지식

- 1단계 검출기가 덜 정확한 주된 까닭은 클래스 불균형이다 <!-- card:c9d0e1f2 -->
  - 근거: 학습 중 쉬운 배경 예가 손실을 덮는다
  - 절: 1
  - 개념: [클래스 불균형](../concepts/클래스-불균형.md), [1단계 검출기](../concepts/1단계-검출기.md)
- Focal Loss 는 잘 맞힌 예의 손실을 줄여 어려운 예에 집중한다 <!-- card:d0e1f2a3 -->
  - 근거: 교차 엔트로피에 (1 − p)^γ 를 곱한다
  - 절: 3
  - 개념: [Focal Loss](../concepts/focal-loss.md)

## 관계

- [RetinaNet](../concepts/retinanet.md) **사용** → [Focal Loss](../concepts/focal-loss.md)
  - 위치: 4절
  - 상태: AI 제안
- [Focal Loss](../concepts/focal-loss.md) **해결** → [클래스 불균형](../concepts/클래스-불균형.md)
  - 설명: 쉬운 예의 손실을 줄여 불균형을 다룬다
  - 위치: 3절
  - 상태: AI 제안
- [RetinaNet](../concepts/retinanet.md) **종류** → [1단계 검출기](../concepts/1단계-검출기.md)
  - 상태: AI 제안
