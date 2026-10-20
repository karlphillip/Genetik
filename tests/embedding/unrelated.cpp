static_assert(__cplusplus==201402L,"unrelated parent standard changed");
#if defined(__SANITIZE_ADDRESS__)
#error Genetik development instrumentation leaked to a sibling
#endif
int main(){return 0;}

#if defined(__has_feature)
#if __has_feature(address_sanitizer)
#error Genetik instrumentation leaked to a Clang sibling
#endif
#endif
