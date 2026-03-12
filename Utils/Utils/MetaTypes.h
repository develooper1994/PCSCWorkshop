#ifndef META_TYPES_H
#define META_TYPES_H

#include "StatusWordHandler.h"
#include <variant>
#include <unordered_map>
#include <string>

namespace MetaTypes {
	// 4 değerden birini alabilir.
	using MetaValue = std::variant<int64_t, uint64_t, std::string, StatusWord>;
	// string anahtarlarıyla MetaValue değerlerini tutan bir harita.
	using MetaMap = std::unordered_map<std::string, MetaValue>;
} // namespace MetaTypes

#endif // !META_TYPES_H
