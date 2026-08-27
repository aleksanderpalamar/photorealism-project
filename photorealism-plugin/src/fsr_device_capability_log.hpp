#pragma once

#include <d3d11.h>

// Registra no log, uma vez por inicializacao de dispositivo, o adaptador DXGI,
// as capacidades de threading e compute, e o suporte aos formatos de cor que o
// pipeline FSR/AA usa. E puramente informativo: nada aqui altera estado do
// dispositivo nem influencia a prova do draw final.
void log_device_capabilities(ID3D11Device* device);
