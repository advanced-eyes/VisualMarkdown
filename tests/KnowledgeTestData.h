#pragma once

#include <QString>

namespace KnowledgeTestData
{
inline const QString kDigestSlot = QStringLiteral("{{digest}}");

// 소스 파일이 CRLF 로 체크아웃돼도 순수 LF 글이 되도록 '\r' 를 없앤다.
inline QString FromRaw(const char* raw)
{
    QString strText = QString::fromUtf8(raw);
    strText.remove(QLatin1Char('\r'));
    return strText;
}

inline QString CanonicalDocument()
{
    return FromRaw(R"MD(---
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
digest: {{digest}}
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

## 관계

- [YOLOv2](../concepts/yolov2.md) **개선** → [YOLO](../concepts/yolo.md)
  - 설명: 배치 정규화 · anchor · 고해상도 학습으로 mAP 63.4 → 76.8, 속도 유지
  - 위치: 2절
  - 인용: "At 67 FPS, YOLOv2 gets 76.8 mAP on VOC 2007."
  - 상태: AI 제안

## 데이터

### YOLO 에서 YOLOv2 까지 — 하나씩 더한 개선

<!-- ocean:data x="단계" y="mAP" ordered="단계" cumulative="yes" concepts="yolov2,yolo" condition="VOC 2007 test" where="표 2" -->

| 단계 | mAP (%) |
| --- | --: |
| YOLO (기준) | 63.4 |
| + 배치 정규화 | 65.8 |
)MD");
}

inline QString CanonicalConcept()
{
    return FromRaw(R"MD(---
type: concept
format: 1
id: yolov2
kind: method
aliases: YOLO9000
year: 2017
---

# YOLOv2

YOLO 에 anchor · 배치 정규화 등을 더해 정확도를 끌어올린 판
)MD");
}
}
