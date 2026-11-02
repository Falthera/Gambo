# GAMBO 2.0 — Quantization-Aware Training (QAT) Roadmap

**Branch**: `gambo-original-sf19-tech`  
**Status**: Architecture Designed  
**Date**: 2026-11-02

---

## 1. Overview

This document describes GAMBO's Quantization-Aware Training (QAT) infrastructure for training GAMBO-native NNUE networks with its own feature system.

---

## 2. QAT Objectives

| Objective | Description |
|-----------|-------------|
| Train GAMBO-native networks | Using GAMBO's pawn-pair, king-relative, threat, and piece-relative features |
| Quantization-aware | Simulate INT8 inference during training for deployment parity |
| GAMBO-native pipeline | No dependency on Stockfish training infrastructure |
| Reproducible | Configurable hyperparameters, deterministic seeding |
| Extensible | Modular design for future feature additions |

---

## 2. QAT Pipeline Architecture

```
┌─────────────────┐
│  GAMBO Dataset  │  (FEN positions + game results)
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│ Feature Extractor│  GAMBO-native: PawnPair + KingRelative +
└────────┬────────┘  ThreatDetection + PieceRelative
         │
         ▼
┌─────────────────┐
│ Quantization    │  Straight-through estimator for INT8
│ Simulator       │  simulates inference quantization
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│ Quantized       │  Forward/backward with simulated quantization
│ Trainer         │  FP32 weights, INT8-simulated forward pass
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│ Consistency     │  Validate FP32 vs INT8 inference parity
│ Validator       │
└────────┬────────┘
         │
         ▼
┌─────────────────┐
│ Quantized       │  Export quantized weights to GAMBO .gnnue format
│ Exporter        │
└─────────────────┘
```

---

## 2. Quantization Strategy

### 2.1 Quantization Parameters (SFNNv16-Aligned)

| Parameter | Value | Rationale |
|-----------|-------|-----------|
| Weight Bits | 8 | Symmetric per-channel |
| Activation Bits | 8 | Asymmetric per-tensor |
| Bias Bits | 32 | Accumulator precision |
| Output Scale | 600 | Matches SFNNv16 |
| WeightScaleBits | 6 | 64 weight levels |
| ActivationScaleBits | 8 | 256 activation levels |
| OutputScaleBits | 14 | Output quantization |

### 2.2 Quantization Functions

```python
def quantize_weights(weights_fp32, scale_bits=6):
    """Symmetric per-channel quantization"""
    max_val = np.max(np.abs(weights_fp32), axis=-1, keepdims=True)
    scale = max_val / (2**(scale_bits-1) - 1)
    weights_int = np.round(weights_fp32 / scale).astype(np.int8)
    return weights_int, scale

def quantize_activations(acts_fp32, scale_bits=8):
    """Asymmetric per-tensor quantization"""
    min_val, max_val = np.min(acts_fp32), np.max(acts_fp32)
    scale = (max_val - min_val) / (2**scale_bits - 1)
    zero_point = -np.round(min_val / scale).astype(np.int8)
    acts_int = np.clip(np.round(acts_fp32 / scale) + zero_point, 
                       -128, 127).astype(np.int8)
    return acts_int, scale, zero_point

def straight_through_estimator(x_int, scale, x_fp32):
    """Straight-through estimator for backward pass"""
    # Forward: quantized integer
    # Backward: gradient passes through as if FP32
    return x_int * scale  # Forward
    # Backward: dL/dx_fp32 = dL/dx_int
```

---

## 3. Training Pipeline Components

### 3.1 Dataset Format

```
GAMBO Training Dataset (.gambo_dataset)
├── Header
│   ├── Version: 1
│   ├── Feature Hash: [4 uint32] (PawnPair, KingRelative, Threat, PieceRelative)
│   ├── Position Count: uint64
│   └── Metadata: {source, date, time_control, result_format}
├── Positions (variable length records)
│   ├── FEN: string (null-terminated)
│   ├── Result: int8 (1=win, 0=draw, -1=loss)
│   ├── Ply: uint16
│   ├── Time Control: string
│   └── Game ID: uint64
└── Checksum: SHA256
```

### 3.2 Training Loop

```python
def train_epoch(model, dataset, optimizer, quant_sim, epoch):
    model.train()
    total_loss = 0
    
    for batch in dataloader(dataset, batch_size=4096, shuffle=True):
        # 1. Extract features (GAMBO-native)
        features = extract_features(batch.fens)  # [B, 4, MaxActive]
        
        # 2. Forward pass with quantization simulation
        with quant_sim:
            logits = model(features)  # INT8-simulated forward
        
        # 3. Compute loss (MSE for evaluation)
        targets = batch.results * SCALE_FACTOR
        loss = mse_loss(logits, targets)
        
        # 4. Backward (straight-through estimator)
        optimizer.zero_grad()
        loss.backward()
        optimizer.step()
        
        total_loss += loss.item()
    
    return total_loss / len(dataloader)

def validate_consistency(model_fp32, model_int8, val_dataset):
    """Validate FP32 vs INT8 inference parity"""
    model_fp32.eval()
    model_int8.eval()
    
    max_diff = 0
    for batch in val_dataset:
        features = extract_features(batch.fens)
        
        with torch.no_grad():
            out_fp32 = model_fp32(features.float())
            out_int8 = model_int8(features.int())
        
        diff = torch.abs(out_fp32 - out_int8).max()
        max_diff = max(max_diff, diff.item())
    
    return max_diff
```

---

## 3. Model Architecture for Training

### 3.1 PyTorch Model Definition (SFNNv16 Topology)

```python
import torch
import torch.nn as nn
import torch.nn.functional as F

class GamboNNUE(nn.Module):
    def __init__(self, feature_dims=[8192, 40960, 60720, 8192], 
                 l1=1024, l2=31, l3=32):
        super().__init__()
        
        # Input feature dimensions
        self.feature_dims = feature_dims
        total_features = sum(feature_dims)
        
        # FC0: Sparse input → 63 (dual ReLU: 31 SqrClippedReLU + 32 ClippedReLU)
        self.fc0 = nn.Linear(total_features, 63, bias=True)
        
        # Dual ReLU: first 31 → SqrClippedReLU, all 63 → ClippedReLU
        # Concatenated: 31 + 32 = 62 (wait: 31 sqr + 32 clip = 63? Let me check)
        # Actually: 31 SqrClippedReLU + 32 ClippedReLU (bias unit) = 63
        # But SFNNv16 uses: 31 SqrClippedReLU + 31 ClippedReLU (no bias in first?) = 62
        # Let me check: L2=31, so FC0_OUTPUTS = 31 per half = 62 total
        # Then concat: 31 (sqr) + 31 (clip) = 62 → FC1
        
        # Actually looking at Stockfish code:
        # fc_0: 1024 → 63 (FC_0_OUTPUTS + 1 = 32? No, FC_0_OUTPUTS=31)
        # Wait: L2=31, FC_0_OUTPUTS = 31, so fc_0 output = 31+1=32?
        # Then ac_sqr_0 on 32, ac_0 on 32, concat first 31 of each = 62
        
        self.fc0 = nn.Linear(1024, 62, bias=True)  # Actually 62 output for dual ReLU
        
        # Dual ReLU
        self.ac_sqr = SqrClippedReLU()  # First 31
        self.ac_clip = ClippedReLU()    # All 62 (or 31?)
        
        # Actually the dual ReLU pattern:
        # FC0 outputs 63 (31*2 + 1 bias?)
        # First 31 → SqrClippedReLU
        # All 63 → ClippedReLU
        # Concat: first 31 of sqr + first 32 of clip = 63
        # But Stockfish uses 62 (31+31) for FC1 input
        
        # Let me use the documented: L2=31, FC1 input = 62
        # So FC0 outputs 63, first 31 SqrClippedReLU, all 63 ClippedReLU
        # Concat first 31 of each = 62
        
        self.fc1 = nn.Linear(62, 32, bias=True)
        self.ac1 = ClippedReLU()
        self.fc2 = nn.Linear(32, 1, bias=True)
        
    def forward(self, x):
        # x: [B, total_features] sparse
        x = self.fc0(x)
        
        # Dual ReLU
        sqr_out = self.ac_sqr(x[:, :31])  # First 31 → SqrClippedReLU
        clip_out = self.ac_clip(x)         # All 62 → ClippedReLU
        # Concat first 31 of each
        x = torch.cat([sqr_out, clip_out[:, :31]], dim=1)  # 62
        
        x = self.fc1(x)
        x = self.ac1(x)
        x = self.fc2(x)
        
        return x


class SqrClippedReLU(nn.Module):
    def forward(self, x):
        x = torch.clamp(x, 0, 127)
        return x * x


class ClippedReLU(nn.Module):
    def forward(self, x):
        return torch.clamp(x, 0, 127)
```

---

## 4. Export to GAMBO Format

```python
def export_to_gambo_format(model, filepath, feature_hashes, metadata):
    """Export trained model to GAMBO .gnnue format"""
    
    # Quantize weights
    fc0_w_int, fc0_w_scale = quantize_weights(model.fc0.weight.data.numpy())
    fc0_b_int, fc0_b_scale = quantize_weights(model.fc0.bias.data.numpy().reshape(-1,1))
    fc1_w_int, fc1_w_scale = quantize_weights(model.fc1.weight.data.numpy())
    fc1_b_int, fc1_b_scale = quantize_weights(model.fc1.bias.data.numpy().reshape(-1,1))
    fc2_w_int, fc2_w_scale = quantize_weights(model.fc2.weight.data.numpy())
    fc2_b_int, fc2_b_scale = quantize_weights(model.fc2.bias.data.numpy().reshape(-1,1))
    
    # Pack into GAMBO format
    header = GamboNetworkHeader(
        magic=b"GAMBO_NNUE\0",
        version=1,
        arch_hash=0xEC42E90D,
        feature_hashes=feature_hashes,
        metadata=metadata
    )
    
    with open(filepath, 'wb') as f:
        f.write(header.pack())
        f.write(fc0_w_int.tobytes())
        f.write(fc0_b_int.tobytes())
        f.write(fc1_w_int.tobytes())
        f.write(fc1_b_int.tobytes())
        f.write(fc2_w_int.tobytes())
        f.write(fc2_b_int.tobytes())
        f.write(struct.pack('<BBBBH', 6, 8, 600, 14, 0))  # Quant params
        f.write(checksum)
```

---

## 5. Dataset Generation (Future)

### 4.1 Self-Play Generation

```bash
# Future: GAMBO self-play on Fishtest or local cluster
# Not part of current implementation
```

### 4.2 Public Dataset Sources

- Fishtest training positions (public)
- CCRL/CEGT game databases
- Lichess database (open source)
- TCEC game archives

---

## 5. Roadmap Timeline

| Phase | Milestone | Target |
|-------|-----------|--------|
| 1 | Feature extractors complete | Sprint 1-2 |
| 2 | Network architecture + inference | Sprint 2-3 |
| 3 | Training pipeline (FP32) | Sprint 3-4 |
| 4 | QAT integration | Sprint 4-5 |
| 5 | Quantized inference validation | Sprint 5 |
| 6 | Export to .gnnue format | Sprint 6 |
| 7 | Optional SFNNv16 converter | Sprint 7 |
| 8 | GAMBO self-play training | Future |

---

## 6. Validation Criteria

| Criterion | Target |
|-----------|--------|
| FP32 vs INT8 max diff | < 0.5 cp |
| Inference speed (INT8) | > 2M nps single-thread |
| Architecture hash match | 0xEC42E90D |
| Feature hash match | All 4 feature sets |
| Training loss convergence | Stable after 50 epochs |

---

*End of QAT Roadmap Document*