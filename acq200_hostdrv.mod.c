#include <linux/module.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>

MODULE_INFO(vermagic, VERMAGIC_STRING);

struct module __this_module
__attribute__((section(".gnu.linkonce.this_module"))) = {
 .name = KBUILD_MODNAME,
 .init = init_module,
#ifdef CONFIG_MODULE_UNLOAD
 .exit = cleanup_module,
#endif
 .arch = MODULE_ARCH_INIT,
};

static const char __module_depends[]
__used
__attribute__((section(".modinfo"))) =
"depends=";

MODULE_ALIAS("pci:v00003388d00000029sv*sd*bc*sc*i*");
MODULE_ALIAS("pci:v00008086d00000318sv*sd*bc*sc*i*");
MODULE_ALIAS("pci:v00008086d00000319sv*sd*bc*sc*i*");

MODULE_INFO(srcversion, "56E0073BDA5F07039D72365");
