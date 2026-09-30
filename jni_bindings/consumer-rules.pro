# GlfwBridge binds its native methods by name through RegisterNatives in
# JNI_OnLoad, and it calls back into itself through jmethodIDs resolved from
# that same class. Shrinking or renaming the class would break both, and the
# failure would be an UnsatisfiedLinkError at game start rather than a build
# error — so the rule ships here and applies to whatever app includes this.
-keep class me.shadow.eclipselauncher.glfw.GlfwBridge { *; }
