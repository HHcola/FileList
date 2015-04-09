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

#define  LOG_TAG    "FileList"
#define  LOGD(...)  __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)


typedef std::vector<std::string> DirEntries;

#define JNIREC_CLASS "com/example/hellojni/HelloJni"  // 指定要注册的类

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




jobjectArray toStringArraybyte(JNIEnv* env, const std::vector<std::string>& strings) {
	LOGD("jni toStringArraybyte");
    size_t count = strings.size();
    jclass byteArrCls = env->FindClass("[B");
    jobjectArray result = env->NewObjectArray(count, byteArrCls, NULL);
    if (result == NULL) {
    	LOGD("toStringArraybyte result is NULL");
        return NULL;
    }

    for (size_t i = 0; i < count; ++i) {
    	std::string strItem = strings.at(i);
    	size_t length = strItem.size();
    	jbyte byteItem[length];
		memset(byteItem,0,length);
    	for(int j = 0; j < length; j ++) {
    		byteItem[j] = strItem.at(j);
    	}
    	jbyteArray byteArray = env->NewByteArray(length);
    	env->SetByteArrayRegion(byteArray, 0, length, byteItem);
    	env->SetObjectArrayElement(result, i, byteArray);
    	env->DeleteLocalRef(byteArray);
    }

    LOGD("jni toStringArraybyte success");
    return result;
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
	LOGD("jni fileList");
    DirEntries entries;
    if (!readDirectory(env, javaPath, entries)) {
        return NULL;
    }
    // Translate the intermediate form into a Java String[].
    return toStringArraybyte(env, entries);
}

// register to ENV
static JNINativeMethod gMethods[] = {
		{"nativeFileListByte", "(Ljava/lang/String;)[[B", (void*) fileList},
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
