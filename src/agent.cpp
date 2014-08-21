#include "agent.h"
#include "modules/echo.h"
#include "log.h"
#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/path.hpp>

#include <json/json.h>


#include <valijson/adapters/jsoncpp_adapter.hpp>
#include <valijson/schema.hpp>
#include <valijson/schema_parser.hpp>
#include <valijson/validation_results.hpp>
#include <valijson/validator.hpp>

namespace puppetlabs {
namespace cthun {

void run_command(std::vector<std::string> command, std::string stdin, std::string &stdout, std::string &stderr) {
    stdout = "{ \"description\": \"schema test\"}";
}


Agent::Agent() {
    modules_["echo"] = std::unique_ptr<Module>(new Modules::Echo);

    // some common schema constants to reduce typing
    valijson::constraints::TypeConstraint json_type_object(valijson::constraints::TypeConstraint::kObject);
    valijson::constraints::TypeConstraint json_type_string(valijson::constraints::TypeConstraint::kString);
    valijson::constraints::TypeConstraint json_type_array(valijson::constraints::TypeConstraint::kArray);

    // action sub-schema
    valijson::Schema action_schema;
    valijson::constraints::PropertiesConstraint::PropertySchemaMap action_properties;
    valijson::constraints::PropertiesConstraint::PropertySchemaMap action_pattern_properties;
    valijson::constraints::RequiredConstraint::RequiredProperties action_required;

    action_schema.addConstraint(json_type_object);

    action_required.insert("name");
    action_properties["name"].addConstraint(json_type_string);

    action_required.insert("input");
    action_properties["input"].addConstraint(json_type_object);

    action_required.insert("output");
    action_properties["output"].addConstraint(json_type_object);

    // constrain the properties to just those in the metadata_properies map
    action_schema.addConstraint(new valijson::constraints::PropertiesConstraint(
                                    action_properties,
                                    action_pattern_properties));

    // specify the required properties
    action_schema.addConstraint(new valijson::constraints::RequiredConstraint(action_required));


    // the metadata schema
    valijson::Schema metadata_schema;
    valijson::constraints::PropertiesConstraint::PropertySchemaMap metadata_properties;
    valijson::constraints::PropertiesConstraint::PropertySchemaMap metadata_pattern_properties;
    valijson::constraints::RequiredConstraint::RequiredProperties metadata_required;

    metadata_schema.addConstraint(json_type_object);

    // description is required
    metadata_required.insert("description");
    metadata_properties["description"].addConstraint(json_type_string);

    // actions is an array of the action sub-schema
    metadata_required.insert("actions");
    metadata_properties["actions"].addConstraint(json_type_array);
    valijson::constraints::ItemsConstraint items_of_type_action_schema(action_schema);
    metadata_properties["actions"].addConstraint(items_of_type_action_schema);

    // constrain the properties to just those in the metadata_properies map
    metadata_schema.addConstraint(new valijson::constraints::PropertiesConstraint(
                                      metadata_properties,
                                      metadata_pattern_properties));

    // specify the required properties
    metadata_schema.addConstraint(new valijson::constraints::RequiredConstraint(metadata_required));


    boost::filesystem::path module_path { "modules" };
    boost::filesystem::directory_iterator end;
    for (auto file = boost::filesystem::directory_iterator(module_path); file != end; ++file) {
        if (!boost::filesystem::is_directory(file->status())) {
            BOOST_LOG_TRIVIAL(info) << file->path().filename().string();

            std::string metadata;
            std::string error;
            run_command({ file->path().filename().string(), "metadata" }, "", metadata, error);

            Json::Value document;
            Json::Reader reader;
            if (!reader.parse(metadata, document)) {
                BOOST_LOG_TRIVIAL(error) << "Parse error: " << reader.getFormatedErrorMessages();
                continue;
            }


            valijson::Validator validator(metadata_schema);
            valijson::adapters::JsonCppAdapter adapted_document(document);
            valijson::ValidationResults validation_results;
            if (!validator.validate(adapted_document, &validation_results)) {
                BOOST_LOG_TRIVIAL(error) << "Failed validation";
                valijson::ValidationResults::Error error;
                while (validation_results.popError(error)) {
                    for (auto path : error.context) {
                        BOOST_LOG_TRIVIAL(error) << path;
                    }
                    BOOST_LOG_TRIVIAL(error) << error.description;
                }

                continue;
            }
            BOOST_LOG_TRIVIAL(info) << "validation OK";
        }
    }
}

void Agent::run() {
}



}  // namespace cthun
}  // namespace puppetlabs
