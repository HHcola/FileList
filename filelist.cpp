#include <jni.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <android/log.h>
#include <assert.h>
#include <vector>

#include <dirent.h>
#include <errno.h>
#include <stddef.h>
#include <exception>

#define  LOG_TAG    "FileList"
#define  LOGD(...)  __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)


typedef std::vector<std::string> DirEntries;


#define JNIREC_CLASS "com/example/hellojni/HelloJni"  // 指定要注册的类

struct VectorCounter {
    const std::vector<std::string>& strings;
    VectorCounter(const std::vector<std::string>& strings) : strings(strings) {}
    size_t operator()() {
        return strings.size();
    }
};
struct VectorGetter {
    const std::vector<std::string>& strings;
    VectorGetter(const std::vector<std::string>& strings) : strings(strings) {}
    const char* operator()(size_t i) {
        return strings[i].c_str();
    }
};

class ScopedUtfChars {
 public:
  ScopedUtfChars(JNIEnv* env, jstring s) : env_(env), string_(s) {
    if (s == NULL) {
      utf_chars_ = NULL;
//      jniThrowNullPointerException(env, NULL);
    } else {
      utf_chars_ = env->GetStringUTFChars(s, NULL);
    }
  }

  ~ScopedUtfChars() {
    if (utf_chars_) {
      env_->ReleaseStringUTFChars(string_, utf_chars_);
    }
  }

  const char* c_str() const {
    return utf_chars_;
  }

  size_t size() const {
    return strlen(utf_chars_);
  }

  const char& operator[](size_t n) const {
    return utf_chars_[n];
  }

 private:
  JNIEnv* env_;
  jstring string_;
  const char* utf_chars_;

  // Disallow copy and assignment.
  ScopedUtfChars(const ScopedUtfChars&);
  void operator=(const ScopedUtfChars&);
};

// Iterates over the filenames in the given directory.
class ScopedReaddir {
public:
    ScopedReaddir(const char* path) {
        mDirStream = opendir(path);
        mIsBad = (mDirStream == NULL);
    }

    ~ScopedReaddir() {
        if (mDirStream != NULL) {
            closedir(mDirStream);
        }
    }

    // Returns the next filename, or NULL.
    const char* next() {
        if (mIsBad) {
            return NULL;
        }
        errno = 0;
        dirent* result = readdir(mDirStream);
        if (result != NULL) {
            return result->d_name;
        }
        if (errno != 0) {
            mIsBad = true;
        }
        return NULL;
    }

    // Has an error occurred on this stream?
    bool isBad() const {
        return mIsBad;
    }

private:
    DIR* mDirStream;
    bool mIsBad;

    // Disallow copy and assignment.
    ScopedReaddir(const ScopedReaddir&);
    void operator=(const ScopedReaddir&);
};


// A smart pointer that deletes a JNI local reference when it goes out of scope.
template<typename T>
class ScopedLocalRef {
public:
    ScopedLocalRef(JNIEnv* env, T localRef) : mEnv(env), mLocalRef(localRef) {
    }

    ~ScopedLocalRef() {
        reset();
    }

    void reset(T ptr = NULL) {
        if (ptr != mLocalRef) {
            if (mLocalRef != NULL) {
                mEnv->DeleteLocalRef(mLocalRef);
            }
            mLocalRef = ptr;
        }
    }

    T release() __attribute__((warn_unused_result)) {
        T localRef = mLocalRef;
        mLocalRef = NULL;
        return localRef;
    }

    T get() const {
        return mLocalRef;
    }

private:
    JNIEnv* mEnv;
    T mLocalRef;

    // Disallow copy and assignment.
    ScopedLocalRef(const ScopedLocalRef&);
    void operator=(const ScopedLocalRef&);
};

static jclass findClass(JNIEnv* env, const char* name) {
    ScopedLocalRef<jclass> localClass(env, env->FindClass(name));
    jclass result = reinterpret_cast<jclass>(env->NewGlobalRef(localClass.get()));
    if (result == NULL) {
    	LOGD("failed to find class '%s'", name);
        abort();
    }
    return result;
}


template <typename Counter, typename Getter>
jobjectArray toStringArray(JNIEnv* env, Counter* counter, Getter* getter) {
    size_t count = (*counter)();
    jobjectArray result = env->NewObjectArray(count, findClass(env, "java/lang/String"), NULL);
    if (result == NULL) {
        return NULL;
    }
    for (size_t i = 0; i < count; ++i) {
    	jstring str = NULL;
    	try {
    		LOGD("jni toStringArray NewStringUTF");
    		str = env->NewStringUTF((*getter)(i));
    	} catch(...) {
    		LOGD("jni toStringArray crash '%d'", i);
    		continue;
    	}
    	if (str == NULL) {
    		continue;
    	}
        ScopedLocalRef<jstring> s(env, str);
//        if (env->ExceptionCheck()) {
//            return NULL;
//        }
        env->SetObjectArrayElement(result, i, s.get());
        if (env->ExceptionCheck()) {
            return NULL;
        }
    }
    return result;
}

jobjectArray toStringArray(JNIEnv* env, const std::vector<std::string>& strings) {
    VectorCounter counter(strings);
    VectorGetter getter(strings);
    return toStringArray<VectorCounter, VectorGetter>(env, &counter, &getter);
}

// Reads the directory referred to by 'pathBytes', adding each directory entry
// to 'entries'.
static bool readDirectory(JNIEnv* env, jstring javaPath, DirEntries& entries) {
    ScopedUtfChars path(env, javaPath);
    if (path.c_str() == NULL) {
        return false;
    }

    ScopedReaddir dir(path.c_str());
    const char* filename;
    while ((filename = dir.next()) != NULL) {
        if (strcmp(filename, ".") != 0 && strcmp(filename, "..") != 0) {
            // TODO: this hides allocation failures from us. Push directory iteration up into Java?
            entries.push_back(filename);
        }
    }
    return !dir.isBad();
}



JNIEXPORT jobjectArray JNICALL fileList( JNIEnv * env, jclass,
		                                  jstring javaPath)
{
    DirEntries entries;
    if (!readDirectory(env, javaPath, entries)) {
        return NULL;
    }
    // Translate the intermediate form into a Java String[].
    return toStringArray(env, entries);
	return NULL;
}

// register to ENV
static JNINativeMethod gMethods[] = {
		{"nativeFileList", "(Ljava/lang/String;)[Ljava/lang/String;", (void*) fileList},
};


static int registerMethods(JNIEnv* env, const char* clzName, JNINativeMethod* methods, int methodCount) {
	jclass clz = env->FindClass(clzName);
	if (clz == NULL) {
		return JNI_FALSE;
	}

	if (env->RegisterNatives(clz, methods, methodCount) < 0) {
		return JNI_FALSE;
	}
	return JNI_TRUE;
}


JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* reserved) {
	LOGD("jni onload");
	JNIEnv* env = NULL;
	if (vm->GetEnv((void**)&env, JNI_VERSION_1_4) != JNI_OK) {
		return -1;
	}
	if (registerMethods(env, JNIREC_CLASS, gMethods, sizeof(gMethods)/ sizeof(gMethods[0])) == JNI_TRUE) {
		LOGD("register sucess");
		return JNI_VERSION_1_4;
	}
	return -1;
}



JNIEXPORT jint JNICALL JNI_UnOnLoad(JavaVM* vm, void* reserved)
{
	JNIEnv* env = NULL;
	jint result = -1;

	LOGD("JNI_UnOnLoad!!");
	return result;
}
