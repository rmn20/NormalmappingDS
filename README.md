# Per-vertex Radiosity Normal Mapping for Nintendo DS  
Based on [SIGGRAPH2007_EfficientSelfShadowedRadiosityNormalMapping.pdf](https://cdn.cloudflare.steamstatic.com/apps/valve/2007/SIGGRAPH2007_EfficientSelfShadowedRadiosityNormalMapping.pdf)  
  
The geometry is rendered 3 times with alpha blending enabled to achieve normalmapping. Original paper uses 3-pass additive blending, but it can be replaced with alpha blending.  
  
Two 32-color textures with 3-bit alpha are used for rendering. (First texture is used twice with alpha blending disabled on the first rendering pass and enabled on the second)  

![image](/images/screenshot.png)
