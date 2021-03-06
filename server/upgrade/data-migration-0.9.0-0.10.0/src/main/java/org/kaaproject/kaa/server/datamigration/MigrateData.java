/*
 * Copyright 2014-2016 CyberVision, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package org.kaaproject.kaa.server.datamigration;

import org.apache.commons.dbutils.DbUtils;
import org.apache.commons.dbutils.QueryRunner;
import org.kaaproject.kaa.server.datamigration.model.Ctl;
import org.kaaproject.kaa.server.datamigration.model.Schema;
import org.kaaproject.kaa.server.datamigration.utils.BaseSchemaIdCounter;
import org.kaaproject.kaa.server.datamigration.utils.DataSources;
import org.kaaproject.kaa.server.datamigration.utils.Options;
import org.kaaproject.kaa.server.common.core.algorithms.generation.ConfigurationGenerationException;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.io.IOException;
import java.sql.Connection;
import java.sql.SQLException;
import java.util.ArrayList;
import java.util.List;
import java.util.Map;

public class MigrateData {

    private static final Logger LOG = LoggerFactory.getLogger(MigrateData.class);
    private static Connection conn;

    public static void main(String[] args) {
        Options options = new Options();

        for (int i = 0; i < args.length; i++) {
            String arg = args[i];
            if (arg.charAt(0) == '-') {
                String option = arg.substring(1, arg.length()).trim();
                if (i >= args.length - 1) {
                    throw new IllegalArgumentException("Not found value after option -" + option);
                }
                switch (option) {
                    case "u":
                        options.setUsername(args[i + 1]);
                        break;
                    case "p":
                        options.setPassword(args[i + 1]);
                        break;
                    case "h":
                        options.setHost(args[i + 1]);
                        break;
                    case "db":
                        options.setDbName(args[i + 1]);
                        break;
                    case "nosql":
                        options.setNoSQL(args[i + 1]);
                        break;
                    case "driver":
                        options.setDriverClassName(args[i + 1]);
                        break;
                    case "url":
                        options.setJdbcUrl(args[i + 1]);
                        break;
                    default:
                        throw new IllegalArgumentException("No such option: -" + option);
                }
            }
        }

        LOG.debug(options.toString());

        try {
            List<Schema> schemas = new ArrayList<>();
            conn = DataSources.getDataSource(options).getConnection();
            QueryRunner runner = new QueryRunner();
            Long maxId = runner.query(conn, "select max(id) as max_id from base_schems", rs -> rs.next() ? rs.getLong("max_id") : null);
            BaseSchemaIdCounter.setInitValue(maxId);
            UpdateUuidsMigration updateUuidsMigration = new UpdateUuidsMigration(conn, options.getHost(), options.getDbName(), options.getNoSQL());
            EndpointProfileMigration endpointProfileMigration = new EndpointProfileMigration(options.getHost(), options.getDbName(), options.getNoSQL());
            List<AbstractCTLMigration> migrationList = new ArrayList<>();
            migrationList.add(new CTLConfigurationMigration(conn));
            migrationList.add(new CTLEventsMigration(conn));
            migrationList.add(new CTLNotificationMigration(conn, options.getHost(), options.getDbName(), options.getNoSQL()));
            migrationList.add(new CTLLogMigration(conn));

            new EndpointSpecificConfigurationMigration(options.getHost(), options.getDbName(), options.getNoSQL()).transform();

            CTLAggregation aggregation = new CTLAggregation(conn);
            BaseSchemaRecordsCreation recordsCreation = new BaseSchemaRecordsCreation(conn);

            // convert uuids from latin1 to base64
            updateUuidsMigration.transform();
            endpointProfileMigration.transform();

            //before phase
            for (AbstractCTLMigration m : migrationList) {
                m.beforeTransform();
            }

            // transform phase
            for (AbstractCTLMigration m : migrationList) {
                schemas.addAll(m.transform());
            }

            //aggregation phase
            Map<Ctl, List<Schema>> ctlToSchemas = aggregation.aggregate(schemas);

            //base schema records creation phase
            recordsCreation.create(ctlToSchemas);

            //after phase
            for (AbstractCTLMigration m : migrationList) {
                m.afterTransform();
            }

            conn.commit();
        } catch (SQLException | IOException | ConfigurationGenerationException e) {
            LOG.error("Error: " + e.getMessage(), e);
            DbUtils.rollbackAndCloseQuietly(conn);
        } finally {
            DbUtils.rollbackAndCloseQuietly(conn);
        }
    }
}
