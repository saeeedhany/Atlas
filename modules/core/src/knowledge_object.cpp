#include "atlas/core/knowledge_object.hpp"

#include <utility>

namespace atlas::core {

Result<KnowledgeObject, ValidationError> KnowledgeObject::create(
    std::string title, std::string definition, std::string problemSolved,
    std::string whyItExists, Difficulty difficulty, ConfidenceLevel confidence) {
    if (title.empty()) {
        return Result<KnowledgeObject, ValidationError>::err(ValidationError::EmptyTitle);
    }
    auto now = std::chrono::system_clock::now();
    StorageRecord record{KnowledgeObjectId::generate(),
                          std::move(title),
                          std::move(definition),
                          std::move(problemSolved),
                          std::move(whyItExists),
                          /*examples=*/{},
                          /*miniProjects=*/{},
                          /*references=*/{},
                          /*notes=*/"",
                          difficulty,
                          confidence,
                          /*createdAt=*/now,
                          /*updatedAt=*/now};
    return Result<KnowledgeObject, ValidationError>::ok(KnowledgeObject(std::move(record)));
}

Result<KnowledgeObject, ValidationError> KnowledgeObject::reconstruct(StorageRecord record) {
    if (record.title.empty()) {
        return Result<KnowledgeObject, ValidationError>::err(ValidationError::EmptyTitle);
    }
    return Result<KnowledgeObject, ValidationError>::ok(KnowledgeObject(std::move(record)));
}

KnowledgeObject::KnowledgeObject(StorageRecord record)
    : id_(record.id),
      title_(std::move(record.title)),
      definition_(std::move(record.definition)),
      problemSolved_(std::move(record.problemSolved)),
      whyItExists_(std::move(record.whyItExists)),
      examples_(std::move(record.examples)),
      miniProjects_(std::move(record.miniProjects)),
      references_(std::move(record.references)),
      notes_(std::move(record.notes)),
      difficulty_(record.difficulty),
      confidence_(record.confidence),
      createdAt_(record.createdAt),
      updatedAt_(record.updatedAt) {}

void KnowledgeObject::touch() { updatedAt_ = std::chrono::system_clock::now(); }

Result<void, ValidationError> KnowledgeObject::renameTo(std::string newTitle) {
    if (newTitle.empty()) {
        return Result<void, ValidationError>::err(ValidationError::EmptyTitle);
    }
    title_ = std::move(newTitle);
    touch();
    return Result<void, ValidationError>::ok();
}

void KnowledgeObject::redefineAs(std::string newDefinition) {
    definition_ = std::move(newDefinition);
    touch();
}

void KnowledgeObject::describeProblemAs(std::string problemSolved) {
    problemSolved_ = std::move(problemSolved);
    touch();
}

void KnowledgeObject::explainWhyItExistsAs(std::string whyItExists) {
    whyItExists_ = std::move(whyItExists);
    touch();
}

void KnowledgeObject::addExample(Example example) {
    examples_.push_back(std::move(example));
    touch();
}

void KnowledgeObject::addMiniProject(MiniProject project) {
    miniProjects_.push_back(std::move(project));
    touch();
}

void KnowledgeObject::addReference(Reference reference) {
    references_.push_back(std::move(reference));
    touch();
}

void KnowledgeObject::setNotes(std::string notes) {
    notes_ = std::move(notes);
    touch();
}

void KnowledgeObject::setDifficulty(Difficulty difficulty) {
    difficulty_ = difficulty;
    touch();
}

void KnowledgeObject::setConfidence(ConfidenceLevel confidence) {
    confidence_ = confidence;
    touch();
}

}  // namespace atlas::core
